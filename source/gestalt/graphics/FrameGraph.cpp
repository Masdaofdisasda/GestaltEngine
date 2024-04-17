#include "FrameGraph.h"

#include <queue>
#include <set>
#include <unordered_set>

#include <tracy/Tracy.hpp>

#include "Gui.h"
#include "GeometryPass.h"
#include "HdrPass.h"
#include "LightingPass.h"
#include "Repository.h"
#include "ShadowPass.h"
#include "SkyboxPass.h"
#include "SsaoPass.h"
#include "VkBootstrap.h"
#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"

namespace gestalt {
  namespace graphics {
    void FrameGraph::init(const Gpu& gpu, const application::Window& window,
                          const std::shared_ptr<ResourceManager>& resource_manager,
                          const std::shared_ptr<foundation::Repository>& repository,
                          const std::shared_ptr<application::Gui>& imgui_gui) {
      gpu_ = gpu;
      window_ = window;
      resource_manager_ = resource_manager;
      repository_ = repository;
      imgui_ = imgui_gui;

      swapchain_->init(gpu_, {window_.extent.width, window_.extent.height, 1});
      commands_->init(gpu_, frames_);
      sync_->init(gpu_, frames_);

      resource_registry_->init(gpu_);

      render_passes_.push_back(
          std::make_unique<DirectionalDepthPass>());
      render_passes_.push_back(std::make_unique<DeferredPass>());

      // render_passes_.push_back(std::make_unique<meshlet_pass>());

      /*
      render_passes_.push_back(std::make_unique<LightingPass>());
      render_passes_.push_back(std::make_unique<SkyboxPass>()); */
      render_passes_.push_back(std::make_unique<InfiniteGridPass>());/*
      render_passes_.push_back(std::make_unique<SsaoFilterPass>());
      render_passes_.push_back(std::make_unique<SsaoBlurPass>());
      render_passes_.push_back(std::make_unique<SsaoFinalPass>());
      render_passes_.push_back(std::make_unique<BrightPass>());
      render_passes_.push_back(std::make_unique<BloomBlurPass>());
      render_passes_.push_back(std::make_unique<StreaksPass>());
      render_passes_.push_back(std::make_unique<LuminancePass>());
      render_passes_.push_back(std::make_unique<LuminanceDownscalePass>());
      render_passes_.push_back(std::make_unique<LightAdaptationPass>());
      render_passes_.push_back(std::make_unique<TonemapPass>());
      render_passes_.push_back(std::make_unique<DebugAabbPass>());*/

      for (int i = 0; i < kFrameOverlap; i++) {
        std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
        };

        frames_[i].descriptor_pool = DescriptorAllocatorGrowable{};
        frames_[i].descriptor_pool.init(gpu_.device, 1000, frame_sizes);
      }

      create_resources();

      for (auto& pass : render_passes_) {
        pass->init(gpu_, resource_manager_, resource_registry_, repository_);
      }

      resource_registry_->clear_shader_cache();
    }

    void FrameGraph::create_resources() {
      for (auto& image_attachment : resource_registry_->attachment_list_) {
        if (image_attachment.extent.width == 0 && image_attachment.extent.height == 0) {
          image_attachment.image->imageExtent
              = {static_cast<uint32_t>(window_.extent.width * image_attachment.scale),
                 static_cast<uint32_t>(window_.extent.height * image_attachment.scale), 1};
        } else {
          image_attachment.image->imageExtent = image_attachment.extent;
        }

        if (image_attachment.image->getType() == TextureType::kColor) {
          resource_manager_->create_color_frame_buffer(image_attachment.image);
        } else if (image_attachment.image->getType() == TextureType::kDepth) {
          resource_manager_->create_depth_frame_buffer(image_attachment.image);
        }
      }
    }

    void FrameGraph::execute(size_t id, VkCommandBuffer cmd) {

        auto renderDependencies = render_passes_.at(id)->get_dependencies();

      for (auto& dependency : renderDependencies.image_attachments) {
        switch (dependency.usage) {
           case ImageUsageType::kRead:
               vkutil::TransitionImage(dependency.attachment.image)
                .toLayoutRead().andSubmitTo(cmd);
          case ImageUsageType::kWrite:
              vkutil::TransitionImage(dependency.attachment.image)
                .toLayoutWrite().andSubmitTo(cmd);
          case ImageUsageType::kDepthStencilRead:
              vkutil::TransitionImage(dependency.attachment.image)
                .toLayoutWrite().andSubmitTo(cmd);
        }
      }

      render_passes_[id]->execute(cmd);

      // increase attachment count TODO

    }

    bool FrameGraph::acquire_next_image() {
      VK_CHECK(
          vkWaitForFences(gpu_.device, 1, &get_current_frame().render_fence, true, UINT64_MAX));

      get_current_frame().descriptor_pool.clear_pools(gpu_.device);
      resource_manager_->descriptor_pool = &get_current_frame().descriptor_pool;

      VkResult e = vkAcquireNextImageKHR(gpu_.device, swapchain_->swapchain, UINT64_MAX,
                                         get_current_frame().swapchain_semaphore, nullptr,
                                         &swapchain_image_index_);
      if (e == VK_ERROR_OUT_OF_DATE_KHR) {
        resize_requested_ = true;
        return true;
      }

      VK_CHECK(vkResetFences(gpu_.device, 1, &get_current_frame().render_fence));
      VK_CHECK(vkResetCommandBuffer(get_current_frame().main_command_buffer, 0));

      return false;
    }

    VkCommandBuffer FrameGraph::start_draw() {
      VkCommandBuffer cmd = get_current_frame().main_command_buffer;
      VkCommandBufferBeginInfo cmdBeginInfo
          = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

      VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

      return cmd;
    }

    void FrameGraph::execute_passes() {
      //create_resources();

      //build_graph();

      if (resize_requested_) {
        swapchain_->resize_swapchain(window_);
        resize_requested_ = false;
      }

      if (acquire_next_image()) {
        return;
      }

      VkCommandBuffer cmd = start_draw();

      for (size_t i = 0; i < render_passes_.size(); i++) {
        ZoneScopedN("Execute Pass");
        execute(i, cmd);
      }

      repository_->main_draw_context_.opaque_surfaces.clear();
      repository_->main_draw_context_.transparent_surfaces.clear();

      const auto color_image = resource_registry_->attachments_.scene_color.image;
      const auto swapchain_image = swapchain_->swapchain_images[swapchain_image_index_];

      vkutil::TransitionImage(color_image)
          .to(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
          .withSource(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT)
          .withDestination(VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT)
          .andSubmitTo(cmd);
      vkutil::TransitionImage(swapchain_image)
          .to(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
          .withSource(VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0)
          .withDestination(VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT)
          .andSubmitTo(cmd);

      vkutil::CopyImage(color_image).toImage(swapchain_image)
          .andSubmitTo(cmd);

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
          .withDestination(VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                           0)
          .andSubmitTo(cmd);
      swapchain_image->setFormat(VK_FORMAT_UNDEFINED);

      present(cmd);
    }

    void FrameGraph::cleanup() {
      for (auto& pass : render_passes_) {
        pass->cleanup();
      }
    }

    void FrameGraph::present(VkCommandBuffer cmd) {
      VK_CHECK(vkEndCommandBuffer(cmd));

      VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
      VkSemaphoreSubmitInfo waitInfo
          = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                          get_current_frame().swapchain_semaphore);
      VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(
          VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame().render_semaphore);

      VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

      VK_CHECK(vkQueueSubmit2(gpu_.graphics_queue, 1, &submit, get_current_frame().render_fence));
      VkPresentInfoKHR presentInfo = vkinit::present_info();
      presentInfo.pSwapchains = &swapchain_->swapchain;
      presentInfo.swapchainCount = 1;
      presentInfo.pWaitSemaphores = &get_current_frame().render_semaphore;
      presentInfo.waitSemaphoreCount = 1;
      presentInfo.pImageIndices = &swapchain_image_index_;

      VkResult presentResult = vkQueuePresentKHR(gpu_.graphics_queue, &presentInfo);
      if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
        resize_requested_ = true;
        return;
      }
    }

    void ResourceRegistry::init(const Gpu& gpu) {
      gpu_ = gpu;

      attachment_list_.push_back(attachments_.scene_color);
      attachment_list_.push_back(attachments_.scene_depth);
      attachment_list_.push_back(attachments_.shadow_map);
      attachment_list_.push_back(attachments_.gbuffer1);
      attachment_list_.push_back(attachments_.gbuffer2);
      attachment_list_.push_back(attachments_.gbuffer3);
    }

    VkShaderModule ResourceRegistry::get_shader(const ShaderProgram& shader_program) {
      const std::string& shader_path = "../shaders/" + shader_program.source_path;

      if (const auto it = shader_cache_.find(shader_path); it != shader_cache_.end()) {
        return it->second;
      }

      VkShaderModule shader_module;
      vkutil::load_shader_module(shader_path.c_str(), gpu_.device, &shader_module);

      shader_cache_[shader_path] = shader_module;

      return shader_module;
    }

    void ResourceRegistry::clear_shader_cache() {
      for (const auto& shader_module : shader_cache_ | std::views::values) {
        vkDestroyShaderModule(gpu_.device, shader_module, nullptr);
      }
      shader_cache_.clear();
    }

    void RenderPass::begin_renderpass(VkCommandBuffer cmd) {
      std::vector<VkRenderingAttachmentInfo> colorAttachments;
      VkRenderingAttachmentInfo depthAttachment = {};
      VkExtent2D extent = {0, 0};

      for (auto& attachment : dependencies_.image_attachments) {
        if (attachment.usage == ImageUsageType::kWrite
            || attachment.usage == ImageUsageType::kDepthStencilRead) {
          std::shared_ptr<TextureHandle>& image = attachment.attachment.image;

          if (image->getType() == TextureType::kColor) {
            if (extent.width == 0 && extent.height == 0) {
              extent = {image->imageExtent.width, image->imageExtent.height};
            }

            if (attachment.clear_operation == ImageClearOperation::kClear) {
              VkClearValue clear = {.color = {0.0f, 0.0f, 0.0f, 1.0f}};
              colorAttachments.push_back(
                  vkinit::attachment_info(image->imageView, &clear, image->getLayout()));
            } else {
              colorAttachments.push_back(
                  vkinit::attachment_info(image->imageView, nullptr, image->getLayout()));
            }
          } else if (image->getType() == TextureType::kDepth) {
            if (extent.width == 0 && extent.height == 0) {
              extent = {image->imageExtent.width, image->imageExtent.height};
            }

            if (attachment.clear_operation == ImageClearOperation::kClear) {
              VkClearValue clear = {.depthStencil = {1.0f, 0}};
              depthAttachment
                  = vkinit::depth_attachment_info(image->imageView, &clear, image->getLayout());
            } else {
              depthAttachment
                  = vkinit::depth_attachment_info(image->imageView, nullptr, image->getLayout());
            }
          }
        }
      }

      viewport_.width = static_cast<float>(extent.width);
      viewport_.height = static_cast<float>(extent.height);
      scissor_.extent.width = extent.width;
      scissor_.extent.height = extent.height;

      VkRenderingInfo renderInfo;

      if (colorAttachments.empty()) {
               renderInfo = vkinit::rendering_info(extent, nullptr, depthAttachment.sType ? &depthAttachment : nullptr);
      } else {
               renderInfo = vkinit::rendering_info_for_gbuffer(extent, colorAttachments.data(), colorAttachments.size(),
                                                              depthAttachment.sType ? &depthAttachment : nullptr);
      }

      vkCmdBeginRendering(cmd, &renderInfo);
    }

    PipelineBuilder RenderPass::create_pipeline() {
      VkShaderModule vertex_shader;
      VkShaderModule fragment_shader;
      std::vector<VkFormat> color_formats;

      // add vertex and fragment shaders
      for (auto& shader_dependency : dependencies_.shaders) {
        if (shader_dependency.stage == ShaderStage::kVertex) {
          vertex_shader = registry_->get_shader(shader_dependency);
        } else if (shader_dependency.stage == ShaderStage::kFragment) {
          fragment_shader = registry_->get_shader(shader_dependency);
        }
      }
      auto builder = PipelineBuilder().set_shaders(vertex_shader, fragment_shader);

      // add color and depth attachments
      for (auto& attachment : dependencies_.image_attachments) {
        if (attachment.usage == ImageUsageType::kWrite
            || attachment.usage == ImageUsageType::kDepthStencilRead) {
          std::shared_ptr<TextureHandle>& image = attachment.attachment.image;

          if (image->getType() == TextureType::kColor) {
            color_formats.push_back(image->getFormat());
          } else if (image->getType() == TextureType::kDepth) {
            builder.set_depth_format(image->getFormat());
          }
        }
      }

      if (!color_formats.empty()) {
        if (color_formats.size() == 1) { // single color attachment
                   builder.set_color_attachment_format(color_formats[0]);
        } else { // multiple color attachments
                   builder.set_color_attachment_formats(color_formats);
        }
      }

      return builder.set_pipeline_layout(pipeline_layout_);
    }

    void VkSwapchain::init(const Gpu& gpu, const VkExtent3D& extent) {
      gpu_ = gpu;

      create_swapchain(extent.width, extent.height);
    }

    void VkSwapchain::create_swapchain(const uint32_t width, const uint32_t height) {
      vkb::SwapchainBuilder swapchainBuilder{gpu_.chosen_gpu, gpu_.device, gpu_.surface};

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
        foundation::TextureHandle handle{};
        handle.image = _swapchainImage;
        handle.setFormat(swapchain_image_format);
        handle.setLayout(VK_IMAGE_LAYOUT_UNDEFINED);
        handle.imageExtent = {swapchain_extent.width, swapchain_extent.height, 1};

        swapchain_images.push_back(std::make_shared<foundation::TextureHandle>(handle));
      }

      const auto views = vkbSwapchain.get_image_views().value();

      for (size_t i = 0; i < views.size(); i++) {
        swapchain_images[i]->imageView = views[i];
      }
    }

    void VkSwapchain::resize_swapchain(application::Window& window) {
      vkDeviceWaitIdle(gpu_.device);

      destroy_swapchain();

      window.update_window_size();

      create_swapchain(window.extent.width, window.extent.height);
    }

    void VkSwapchain::destroy_swapchain() const {
      vkDestroySwapchainKHR(gpu_.device, swapchain, nullptr);

      // destroy swapchain resources
      for (auto& _swapchainImage : swapchain_images) {
        vkDestroyImageView(gpu_.device, _swapchainImage->imageView, nullptr);
      }
    }
  }  // namespace graphics
}  // namespace gestalt