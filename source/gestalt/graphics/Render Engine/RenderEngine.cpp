﻿#include "RenderEngine.hpp"

#include <queue>
#include <set>

#include <tracy/Tracy.hpp>

#include "Gui.hpp"
#include "Renderpasses/RenderPassTypes.hpp"
#include "Repository.hpp"
#include <VkBootstrap.h>

#include "VulkanCheck.hpp"

#include "FrameProvider.hpp"
#include "RenderPassBase.hpp"
#include "ResourceAllocator.hpp"
#include "ResourceManager.hpp"
#include "Utils/vk_images.hpp"
#include "vk_initializers.hpp"
#include "Interface/IGpu.hpp"

namespace gestalt::graphics {
  void RenderEngine::init(IGpu* gpu, Window* window,
                            ResourceManager* resource_manager, ResourceAllocator* resource_allocator,
                            Repository* repository, Gui* imgui_gui, FrameProvider* frame) {

    gpu_ = gpu;
    window_ = window;
    resource_manager_ = resource_manager;
    resource_allocator_ = resource_allocator;
    repository_ = repository;
    imgui_ = imgui_gui;
    frame_ = frame;

    swapchain_ = std::make_unique<VkSwapchain>();
    swapchain_->init(gpu_, {window_->extent.width, window_->extent.height, 1});

    resource_registry_ = std::make_unique<ResourceRegistry>();
    resource_registry_->init(gpu_, repository_);

    frame_graph_ = std::make_unique<fg::FrameGraph>(resource_allocator);

    {
      auto shadow_map = frame_graph_->add_resource(
          ImageTemplate("shadow_map")
              .set_image_type(TextureType::kDepth, VK_FORMAT_D32_SFLOAT)
              .set_image_size(2048 * 4, 2048 * 4)
              .build());
      auto g_buffer_1 = frame_graph_->add_resource(ImageTemplate("g_buffer1"));
      auto g_buffer_2 = frame_graph_->add_resource(ImageTemplate("g_buffer2"));
      auto g_buffer_3 = frame_graph_->add_resource(ImageTemplate("g_buffer3"));
      auto g_buffer_depth = frame_graph_->add_resource(
          ImageTemplate("g_buffer_depth")
              .set_image_type(TextureType::kDepth, VK_FORMAT_D32_SFLOAT)
              .build());
      auto scene_lit = frame_graph_->add_resource(ImageTemplate("scene_lit"));
      auto scene_final = frame_graph_->add_resource(ImageTemplate("scene_final"));
      auto rotation_texture = frame_graph_->add_resource(
          ImageTemplate("rotation_texture")
              .set_initial_value(std::filesystem::current_path() / "../../assets/rot_texture.bmp")
              .build(),
          fg::CreationType::EXTERNAL);
      auto occlusion_texture = frame_graph_->add_resource(
          ImageTemplate("occlusion_texture")
              .set_image_type(TextureType::kColor, VK_FORMAT_R16_SFLOAT)
              .set_image_size(0.5f)
              .build());

      // camera
      auto camera_buffer = frame_graph_->add_resource(
          repository->per_frame_data_buffers->uniform_buffers_instance);

      // geometry
      auto index_buffer = frame_graph_->add_resource(
          repository->mesh_buffers->index_buffer_instance, fg::CreationType::EXTERNAL);
      auto vertex_position_buffer
          = frame_graph_->add_resource(repository->mesh_buffers->vertex_position_buffer_instance);
      auto vertex_data_buffer
          = frame_graph_->add_resource(repository->mesh_buffers->vertex_data_buffer_instance);

      auto meshlet_buffer
          = frame_graph_->add_resource(repository->mesh_buffers->meshlet_buffer_instance);
      auto meshlet_vertices
          = frame_graph_->add_resource(repository->mesh_buffers->meshlet_vertices_instance);
      auto meshlet_triangles
          = frame_graph_->add_resource(repository->mesh_buffers->meshlet_triangles_instance);
      auto meshlet_task_commands_buffer = frame_graph_->add_resource(
          repository->mesh_buffers->meshlet_task_commands_buffer_instance);
      auto mesh_draw_buffer
          = frame_graph_->add_resource(repository->mesh_buffers->mesh_draw_buffer_instance);
      auto draw_count_buffer
          = frame_graph_->add_resource(repository->mesh_buffers->draw_count_buffer_instance);
      // Material
      auto material_buffer
          = frame_graph_->add_resource(repository->material_buffers->material_buffer);

      // Light
      auto directional_light
          = frame_graph_->add_resource(repository->light_buffers->dir_light_buffer_instance);
      auto point_light
          = frame_graph_->add_resource(repository->light_buffers->point_light_buffer_instance);
      auto light_matrices
          = frame_graph_->add_resource(repository->light_buffers->view_proj_matrices_instance);

      // Shader Passes
      auto draw_cull_directional_depth_pass = std::make_shared<fg::DrawCullDirectionalDepthPass>(
          camera_buffer, meshlet_task_commands_buffer, mesh_draw_buffer, draw_count_buffer, gpu_);
      auto task_submit_directional_depth_pass
          = std::make_shared<fg::TaskSubmitDirectionalDepthPass>(meshlet_task_commands_buffer,
                                                                 draw_count_buffer, gpu_);
      auto meshlet_directional_depth_pass = std::make_shared<fg::MeshletDirectionalDepthPass>(
          camera_buffer, light_matrices, directional_light, point_light, vertex_position_buffer,
          vertex_data_buffer, meshlet_buffer, meshlet_vertices, meshlet_triangles,
          meshlet_task_commands_buffer, mesh_draw_buffer, shadow_map, gpu_);

      auto geometry_pass = std::make_shared<fg::GeometryPass>(
          g_buffer_1, g_buffer_2, g_buffer_3, g_buffer_depth, vertex_position_buffer, gpu_);
      auto lighting_pass = std::make_shared<fg::LightingPass>(
          scene_lit, g_buffer_1, g_buffer_2, g_buffer_3, g_buffer_depth, shadow_map,
          material_buffer, directional_light, gpu_);
      auto tone_map_pass = std::make_shared<fg::ToneMapPass>(scene_final, scene_lit, gpu_);
      auto ssao_pass = std::make_shared<fg::SsaoPass>(scene_lit, g_buffer_depth, rotation_texture,
                                                      occlusion_texture, gpu_);

      frame_graph_->add_render_pass(std::move(draw_cull_directional_depth_pass));
      frame_graph_->add_render_pass(std::move(task_submit_directional_depth_pass));
      frame_graph_->add_render_pass(std::move(meshlet_directional_depth_pass));
      frame_graph_->add_render_pass(std::move(geometry_pass));
      frame_graph_->add_render_pass(std::move(lighting_pass));
      frame_graph_->add_render_pass(std::move(tone_map_pass));
      frame_graph_->add_render_pass(std::move(ssao_pass));

      frame_graph_->compile();
    }

    render_passes_.push_back(std::make_shared<DrawCullDirectionalDepthPass>());
    render_passes_.push_back(std::make_shared<TaskSubmitDirectionalDepthPass>());
    render_passes_.push_back(std::make_shared<MeshletDirectionalDepthPass>());

    render_passes_.push_back(std::make_shared<DrawCullPass>());
    render_passes_.push_back(std::make_shared<TaskSubmitPass>());
    render_passes_.push_back(std::make_unique<MeshletPass>());

    render_passes_.push_back(std::make_shared<SsaoPass>());
    render_passes_.push_back(std::make_shared<SsaoBlurPass>());

    render_passes_.push_back(std::make_shared<VolumetricLightingNoisePass>());
    render_passes_.push_back(std::make_shared<VolumetricLightingInjectionPass>());
    render_passes_.push_back(std::make_shared<VolumetricLightingScatteringPass>());
    render_passes_.push_back(std::make_shared<VolumetricLightingSpatialFilterPass>());
    render_passes_.push_back(std::make_shared<VolumetricLightingIntegrationPass>());

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

    frames_.init(gpu_->getDevice(), gpu_->getGraphicsQueueFamily(), *frame_);

    create_resources();

    for (const auto& pass : render_passes_) {
      pass->init(gpu_, resource_manager_, resource_registry_.get(), repository_, frame_);
    }

    resource_registry_->clear_shader_cache();
  }

  void RenderEngine::create_resources() const {
    resource_manager_->flush_loader();

    gpu_->immediateSubmit([&](VkCommandBuffer cmd) {
      for (auto& image_attachment : resource_registry_->attachment_list_) {
        if (image_attachment.extent.width == 0 && image_attachment.extent.height == 0) {
          image_attachment.image->imageExtent
              = {static_cast<uint32_t>(window_->extent.width * image_attachment.scale),
                 static_cast<uint32_t>(window_->extent.height * image_attachment.scale), 0};
        } else {
          image_attachment.image->imageExtent = image_attachment.extent;
        }

        if (image_attachment.extent.depth != 0) {
          resource_manager_->create_3D_image(
              image_attachment.image, image_attachment.extent,
                                             image_attachment.format,
                                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, image_attachment.name);
        } else {

         resource_manager_->create_frame_buffer(image_attachment.image, image_attachment.name, image_attachment.format);
        }

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

  void RenderEngine::execute(const std::shared_ptr<RenderPassBase>& render_pass,
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
        case ImageUsageType::kStore:
          vkutil::TransitionImage(dependency.attachment.image).toLayoutStore().andSubmitTo(cmd);
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
      debug_texture_ = std::make_shared<TextureHandleOld>(copyImg.image->getType());
      debug_texture_->imageExtent = copyImg.image->imageExtent;
      resource_manager_->create_frame_buffer(debug_texture_, "debug_texture");

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

  bool RenderEngine::acquire_next_image() {
    VK_CHECK(vkWaitForFences(gpu_->getDevice(), 1, &frame().render_fence, true, UINT64_MAX));
    vkDeviceWaitIdle(gpu_->getDevice()); // TODO fix synchronization issues!!!

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

  VkCommandBuffer RenderEngine::start_draw() {
    VkCommandBuffer cmd = frame().main_command_buffer;
    VkCommandBufferBeginInfo cmdBeginInfo
        = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    return cmd;
  }

  void RenderEngine::execute_passes() {

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

      // TODO
    const VkCommandBuffer command_buffer = nullptr;
    const fg::CommandBuffer cmd_buffer{command_buffer};
    frame_graph_->execute(cmd_buffer);

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

    present(cmd);
  }

  void RenderEngine::cleanup() {
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

  void RenderEngine::present(VkCommandBuffer cmd) {
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

}  // namespace gestalt::graphics