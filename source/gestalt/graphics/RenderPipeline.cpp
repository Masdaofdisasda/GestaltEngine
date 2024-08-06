#include "RenderPipeline.hpp"

#include <queue>
#include <set>
#include <unordered_set>

#include <tracy/Tracy.hpp>

#include "Gui.hpp"
#include "RenderPass.hpp"
#include "Repository.hpp"
#include <VkBootstrap.h>

#include "vk_images.hpp"
#include "vk_initializers.hpp"

namespace gestalt::graphics {
  void RenderPipeline::init(IGpu* gpu, Window* window,
                            ResourceManager* resource_manager,
                            Repository* repository, Gui* imgui_gui, FrameProvider* frame) {
    gpu_ = gpu;
    window_ = window;
    resource_manager_ = resource_manager;
    repository_ = repository;
    imgui_ = imgui_gui;
    frame_ = frame;

    swapchain_->init(gpu_, {window_->extent.width, window_->extent.height, 1});

    resource_registry_->init(gpu_, repository_);

    render_passes_.push_back(std::make_shared<DirectionalDepthPass>());
    // render_passes_.push_back(std::make_shared<DeferredPass>());
    render_passes_.push_back(std::make_shared<DrawCullPass>());
    render_passes_.push_back(std::make_shared<TaskSubmitPass>());
    render_passes_.push_back(std::make_unique<MeshletPass>());
    render_passes_.push_back(std::make_shared<LightingPass>());
    render_passes_.push_back(std::make_shared<SkyboxPass>());

    render_passes_.push_back(std::make_shared<LuminancePass>());
    render_passes_.push_back(std::make_shared<LuminanceDownscalePass>());
    render_passes_.push_back(std::make_shared<LightAdaptationPass>());
    render_passes_.push_back(std::make_shared<BrightPass>());
    render_passes_.push_back(std::make_shared<BloomBlurPass>());
    render_passes_.push_back(std::make_shared<InfiniteGridPass>());
    render_passes_.push_back(std::make_shared<AabbBvhDebugPass>());
    render_passes_.push_back(std::make_shared<BoundingSphereDebugPass>());
    render_passes_.push_back(std::make_shared<TonemapPass>());

    frames_.init(gpu_->getDevice(), gpu_->getGraphicsQueueFamily(), frame_);

    create_resources();

    for (const auto& pass : render_passes_) {
      pass->init(gpu_, resource_manager_, resource_registry_.get(), repository_, frame_);
    }

    resource_registry_->clear_shader_cache();
  }

  void RenderPipeline::create_resources() const {
    resource_manager_->flush_loader();

    gpu_->immediateSubmit([&](VkCommandBuffer cmd) {
      for (auto& image_attachment : resource_registry_->attachment_list_) {
        if (image_attachment.extent.width == 0 && image_attachment.extent.height == 0) {
          image_attachment.image->imageExtent
              = {static_cast<uint32_t>(window_->extent.width * image_attachment.scale),
                 static_cast<uint32_t>(window_->extent.height * image_attachment.scale), 1};
        } else {
          image_attachment.image->imageExtent = image_attachment.extent;
        }

        resource_manager_->create_frame_buffer(image_attachment.image);
        if (image_attachment.image->getType() == TextureType::kColor) {
          vkutil::TransitionImage(image_attachment.image)
              .to(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
              .withSource(VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0)
              .withDestination(VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT)
              .andSubmitTo(cmd);

          VkImageSubresourceRange clearRange = {};
          clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
          clearRange.baseMipLevel = 0;
          clearRange.levelCount = 1;
          clearRange.baseArrayLayer = 0;
          clearRange.layerCount = 1;
          vkCmdClearColorImage(cmd, image_attachment.image->image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               &image_attachment.initial_value.color, 1, &clearRange);

        } else if (image_attachment.image->getType() == TextureType::kDepth) {
          vkutil::TransitionImage(image_attachment.image)
              .to(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
              .withSource(VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0)
              .withDestination(VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT)
              .andSubmitTo(cmd);

          VkImageSubresourceRange clearRange = {};
          clearRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
          if (image_attachment.image->getFormat() == VK_FORMAT_D32_SFLOAT_S8_UINT
              || image_attachment.image->getFormat() == VK_FORMAT_D24_UNORM_S8_UINT) {
            clearRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
          }
          clearRange.baseMipLevel = 0;
          clearRange.levelCount = 1;
          clearRange.baseArrayLayer = 0;
          clearRange.layerCount = 1;
          vkCmdClearDepthStencilImage(cmd, image_attachment.image->image,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      &image_attachment.initial_value.depthStencil, 1, &clearRange);
        }
      }
    });
  }

  void RenderPipeline::execute(const std::shared_ptr<RenderPass>& render_pass,
                               VkCommandBuffer cmd) {
    auto renderDependencies = render_pass->get_dependencies();

    for (auto& dependency : renderDependencies.image_attachments) {
      switch (dependency.usage) {
        case ImageUsageType::kRead:
          vkutil::TransitionImage(dependency.attachment.image).toLayoutRead().andSubmitTo(cmd);
          break;
        case ImageUsageType::kWrite:
          vkutil::TransitionImage(dependency.attachment.image).toLayoutWrite().andSubmitTo(cmd);
          dependency.attachment.version++;
          break;
        case ImageUsageType::kDepthStencilRead:
          vkutil::TransitionImage(dependency.attachment.image).toLayoutWrite().andSubmitTo(cmd);
          break;
        case ImageUsageType::kCombined:
          dependency.attachment.version++;
          break;
      }
    }

    for (auto& dependency : renderDependencies.buffer_dependencies) {
      switch (dependency.usage) {
        case BufferUsageType::kRead:
          for (auto& resource :
               dependency.resource.buffer->get_buffers(frame_->get_current_frame_index()))
            vkutil::TransitionBuffer(resource).waitForRead().andSubmitTo(cmd);
          break;
        case BufferUsageType::kWrite:
          for (auto& resource : dependency.resource.buffer->get_buffers(frame_->get_current_frame_index()))
            vkutil::TransitionBuffer(resource).waitForWrite().andSubmitTo(cmd);
          break;
      }
    }

    //fmt::print("Executing {}\n", render_pass->get_name());
    render_pass->execute(cmd);

    if (false && render_pass->get_name() == "Bloom Blur Pass") {
      if (debug_texture_ != nullptr) return;

      const auto copyImg = resource_registry_->resources_.bright_pass;
      debug_texture_ = std::make_shared<TextureHandle>(copyImg.image->getType());
      debug_texture_->imageExtent = copyImg.image->imageExtent;
      resource_manager_->create_frame_buffer(debug_texture_);

      vkutil::TransitionImage(copyImg.image)
          .to(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
          .withSource(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT)
          .withDestination(VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT)
          .andSubmitTo(cmd);
      vkutil::TransitionImage(debug_texture_)
          .to(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
          .withSource(VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0)
          .withDestination(VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT)
          .andSubmitTo(cmd);

      vkutil::CopyImage(copyImg.image).toImage(debug_texture_).andSubmitTo(cmd);

      vkutil::TransitionImage(debug_texture_)
          .toLayoutRead()
          .withSource(VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                      VK_ACCESS_2_TRANSFER_WRITE_BIT)  // Wait for the copy to finish
          .andSubmitTo(cmd);

      imgui_->set_debug_texture(copyImg.image->imageView,
                                repository_->default_material_.color_sampler);
    }
  }

  bool RenderPipeline::acquire_next_image() {
    VK_CHECK(vkWaitForFences(gpu_->getDevice(), 1, &frame().render_fence, true, UINT64_MAX));

    VkResult e
        = vkAcquireNextImageKHR(gpu_->getDevice(), swapchain_->swapchain, UINT64_MAX,
                                frame().swapchain_semaphore, nullptr, &swapchain_image_index_);
    if (e == VK_ERROR_OUT_OF_DATE_KHR) {
      resize_requested_ = true;
      return true;
    }

    VK_CHECK(vkResetFences(gpu_->getDevice(), 1, &frame().render_fence));
    VK_CHECK(vkResetCommandBuffer(frame().main_command_buffer, 0));

    return false;
  }

  VkCommandBuffer RenderPipeline::start_draw() {
    VkCommandBuffer cmd = frame().main_command_buffer;
    VkCommandBufferBeginInfo cmdBeginInfo
        = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    return cmd;
  }

  void RenderPipeline::execute_passes() {
    FrameGraphWIP frame_graph{render_passes_};
    frame_graph.topologicalSort();

    if (resize_requested_) {
      swapchain_->resize_swapchain(window_);
      resize_requested_ = false;
    }

    if (acquire_next_image()) {
      return;
    }

    VkCommandBuffer cmd = start_draw();

    for (auto& renderpass : render_passes_) {
      ZoneScopedN("Execute Pass");
      execute(renderpass, cmd);
    }

    const auto color_image = resource_registry_->resources_.final_color.image;
    const auto swapchain_image = swapchain_->swapchain_images[swapchain_image_index_];

    vkutil::TransitionImage(color_image)
        .to(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
        .withSource(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT)
        .withDestination(VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT)
        .andSubmitTo(cmd);
    vkutil::TransitionImage(swapchain_image)
        .to(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        .withSource(VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0)
        .withDestination(VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT)
        .andSubmitTo(cmd);

    vkutil::CopyImage(color_image).toImage(swapchain_image).andSubmitTo(cmd);

    vkutil::TransitionImage(swapchain_image)
        .to(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .withSource(VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT)  // Wait for the copy to finish
        .withDestination(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT)
        .andSubmitTo(cmd);

    imgui_->draw(cmd, swapchain_image);

    vkutil::TransitionImage(swapchain_image)
        .to(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
        .withSource(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT)
        .withDestination(VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0)
        .andSubmitTo(cmd);
    swapchain_image->setFormat(VK_FORMAT_UNDEFINED);

    /*
    vkutil::TransitionBuffer(perFrameDataBuffer).waitForRead().andSubmitTo(cmd);
    vkutil::TransitionBuffer(meshBuffers.index_buffer).waitForRead().andSubmitTo(cmd);
    vkutil::TransitionBuffer(meshBuffers.vertex_position_buffer).waitForRead().andSubmitTo(cmd);
    vkutil::TransitionBuffer(meshBuffers.vertex_data_buffer).waitForRead().andSubmitTo(cmd);
    vkutil::TransitionBuffer(meshBuffers.meshlet_buffer).waitForRead().andSubmitTo(cmd);
    vkutil::TransitionBuffer(meshBuffers.meshlet_vertices).waitForRead().andSubmitTo(cmd);
    vkutil::TransitionBuffer(meshBuffers.meshlet_triangles).waitForRead().andSubmitTo(cmd);
    vkutil::TransitionBuffer(meshBuffers.mesh_draw_buffer[frames_.get_current_frame_index()])
        .waitForRead()
        .andSubmitTo(cmd);
    vkutil::TransitionBuffer(lightBuffers.dir_light_buffer).waitForRead().andSubmitTo(cmd);
    vkutil::TransitionBuffer(lightBuffers.point_light_buffer).waitForRead().andSubmitTo(cmd);
    vkutil::TransitionBuffer(lightBuffers.view_proj_matrices).waitForRead().andSubmitTo(cmd);
    vkutil::TransitionBuffer(materialData.uniform_buffer);*/

    present(cmd);
  }

  void RenderPipeline::cleanup() {
    for (auto& attachment : resource_registry_->attachment_list_) {
      resource_manager_->destroy_image(*attachment.image.get());
    }
    resource_registry_->attachment_list_.clear();

    frames_.cleanup(gpu_->getDevice());

    for (const auto& pass : render_passes_) {
      pass->cleanup();
    }
    render_passes_.clear();

    swapchain_->destroy_swapchain();
  }

  void RenderPipeline::present(VkCommandBuffer cmd) {
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, frame().swapchain_semaphore);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, frame().render_semaphore);

    VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

    VK_CHECK(vkQueueSubmit2(gpu_->getGraphicsQueue(), 1, &submit, frame().render_fence));
    VkPresentInfoKHR presentInfo = vkinit::present_info();
    presentInfo.pSwapchains = &swapchain_->swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &frame().render_semaphore;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices = &swapchain_image_index_;

    VkResult presentResult = vkQueuePresentKHR(gpu_->getGraphicsQueue(), &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
      resize_requested_ = true;
    }
  }

  void VkSwapchain::init(IGpu* gpu, const VkExtent3D& extent) {
    gpu_ = gpu;

    create_swapchain(extent.width, extent.height);
  }

  void VkSwapchain::create_swapchain(const uint32_t width, const uint32_t height) {
    vkb::SwapchainBuilder swapchainBuilder{gpu_->getPhysicalDevice(), gpu_->getDevice(),
                                           gpu_->getSurface()};

    swapchain_image_format = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain = swapchainBuilder
                                      //.use_default_format_selection()
                                      .set_desired_format(VkSurfaceFormatKHR{
                                          .format = swapchain_image_format,
                                          .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
                                      // use vsync present mode
                                      .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                                      .set_desired_extent(width, height)
                                      .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                                      .set_desired_min_image_count(2)
                                      .build()
                                      .value();

    swapchain_extent = vkbSwapchain.extent;
    // store swapchain and its related images
    swapchain = vkbSwapchain.swapchain;

    for (const auto& _swapchainImage : vkbSwapchain.get_images().value()) {
      TextureHandle handle{};
      handle.image = _swapchainImage;
      handle.setFormat(swapchain_image_format);
      handle.setLayout(VK_IMAGE_LAYOUT_UNDEFINED);
      handle.imageExtent = {swapchain_extent.width, swapchain_extent.height, 1};

      swapchain_images.push_back(std::make_shared<TextureHandle>(handle));
    }

    const auto views = vkbSwapchain.get_image_views().value();

    for (size_t i = 0; i < views.size(); i++) {
      swapchain_images[i]->imageView = views[i];
    }
  }

  void VkSwapchain::resize_swapchain(Window* window) {
    vkDeviceWaitIdle(gpu_->getDevice());

    destroy_swapchain();

    window->update_window_size();

    create_swapchain(window->extent.width, window->extent.height);
  }

  void VkSwapchain::destroy_swapchain() const {
    vkDestroySwapchainKHR(gpu_->getDevice(), swapchain, nullptr);

    // destroy swapchain resources
    for (auto& _swapchainImage : swapchain_images) {
      vkDestroyImageView(gpu_->getDevice(), _swapchainImage->imageView, nullptr);
    }
  }

  FrameGraphWIP::FrameGraphWIP(const std::vector<std::shared_ptr<RenderPass>>& render_passes) {
    for (const auto& pass : render_passes) {
      auto deps = pass->get_dependencies();
      // Update the writer map and track versions
      for (const auto& dep : deps.image_attachments) {
        if (dep.usage == ImageUsageType::kWrite || dep.usage == ImageUsageType::kCombined) {
          writer_map[dep.attachment.image] = {pass, dep.attachment.version + 1};
        }
      }

      // Setup edges based on reading dependencies
      for (const auto& dep : deps.image_attachments) {
        if (dep.usage == ImageUsageType::kRead || dep.usage == ImageUsageType::kCombined
            || dep.usage == ImageUsageType::kDepthStencilRead) {
          auto it = writer_map.find(dep.attachment.image);
          if (it != writer_map.end() && it->second.version == dep.attachment.version) {
            AddEdge(it->second.pass, pass);
          }
        }
      }
    }
  }

  void FrameGraphWIP::AddEdge(const std::shared_ptr<RenderPass>& u,
                              const std::shared_ptr<RenderPass>& v) {
    adj_[u].insert(v);
    in_degree_[v]++;
  }

  void FrameGraphWIP::topologicalSort() {
    std::deque<std::shared_ptr<RenderPass>> queue;

    // Find all vertices with in-degree 0
    for (auto& node : in_degree_) {
      if (node.second == 0 && adj_.find(node.first) != adj_.end()) {
        queue.push_back(node.first);
      }
    }

    while (!queue.empty()) {
      std::shared_ptr<RenderPass> v = queue.front();
      queue.pop_front();
      sorted_passes_.push_back(v);

      // For each node u adjacent to v in the graph
      for (auto u : adj_[v]) {
        in_degree_[u]--;
        if (in_degree_[u] == 0) {
          queue.push_back(u);
        }
      }
    }

    if (sorted_passes_.size() != adj_.size()) {
      throw std::runtime_error("Cycle detected in the graph, cannot perform topological sort.");
    }
  }

  std::vector<std::shared_ptr<RenderPass>> FrameGraphWIP::get_sorted_passes() const {
    return sorted_passes_;
  }
}  // namespace gestalt::graphics