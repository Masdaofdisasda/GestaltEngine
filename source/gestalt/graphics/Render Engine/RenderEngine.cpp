#include "RenderEngine.hpp"

#include <queue>

#include <tracy/Tracy.hpp>

#include "Gui.hpp"
#include "Repository.hpp"
#include <VkBootstrap.h>

#include "VulkanCheck.hpp"

#include "FrameProvider.hpp"
#include "ResourceAllocator.hpp"
#include "ResourceManager.hpp"
#include "Utils/vk_images.hpp"
#include "vk_initializers.hpp"
#include "Interface/IGpu.hpp"

namespace gestalt::graphics {

  inline std::filesystem::path asset(const std::string& asset_name) {
    return std::filesystem::current_path() / "../../assets" / asset_name;
  }

  void RenderEngine::init(IGpu* gpu, Window* window, ResourceAllocator* resource_allocator,
                            Repository* repository, Gui* imgui_gui, FrameProvider* frame) {

    gpu_ = gpu;
    window_ = window;
    resource_allocator_ = resource_allocator;
    repository_ = repository;
    imgui_ = imgui_gui;
    frame_ = frame;

    swapchain_ = std::make_unique<VkSwapchain>();
    swapchain_->init(gpu_, {window_->extent.width, window_->extent.height, 1});

    frame_graph_ = std::make_unique<fg::FrameGraph>(resource_allocator);

    {
      auto shadow_map = frame_graph_->add_resource(
          ImageTemplate("Shadow Map")
              .set_image_type(TextureType::kDepth, VK_FORMAT_D32_SFLOAT)
              .set_image_size(2048 * 4, 2048 * 4)
              .build());
      auto g_buffer_1 = frame_graph_->add_resource(ImageTemplate("G Buffer 1"));
      auto g_buffer_2 = frame_graph_->add_resource(ImageTemplate("G Buffer 2"));
      auto g_buffer_3 = frame_graph_->add_resource(ImageTemplate("G Buffer 3"));
      auto g_buffer_depth = frame_graph_->add_resource(
          ImageTemplate("G Buffer Depth")
              .set_image_type(TextureType::kDepth, VK_FORMAT_D32_SFLOAT)
              .build());
      auto scene_lit = frame_graph_->add_resource(ImageTemplate("Scene Lit"));
      auto scene_skybox = frame_graph_->add_resource(ImageTemplate("Scene Skybox"));
      auto scene_final = frame_graph_->add_resource(ImageTemplate("Scene Final"));
      scene_final_ = scene_final;
      auto rotation_texture
          = frame_graph_->add_resource(ImageTemplate("Rotation Pattern Texture")
                                           .set_initial_value(asset("rot_texture.bmp"))
                                           .build(),
                                       fg::CreationType::EXTERNAL);
      auto ambient_occlusion_texture = frame_graph_->add_resource(
          ImageTemplate("Ambient Occlusion Texture")
              .set_image_type(TextureType::kColor, VK_FORMAT_R16_SFLOAT)
              .set_image_size(0.5f)
              .build());

      auto blue_noise
          = frame_graph_->add_resource(ImageTemplate("Blue Noise Texture")
                                           .set_initial_value(asset("blue_noise_512_512.png"))
                                           .build(),
                                       fg::CreationType::EXTERNAL);
      auto froxel_data = frame_graph_->add_resource(
          ImageTemplate("Froxel Data 0")
              .set_image_type(TextureType::kColor, VK_FORMAT_R16G16B16A16_SFLOAT,
                              ImageType::kImage3D)
              .set_image_size(128, 128, 128)
              .build());
      auto light_scattering = frame_graph_->add_resource(
          ImageTemplate("Light Scattering Texture")
              .set_image_type(TextureType::kColor, VK_FORMAT_R16G16B16A16_SFLOAT,
                              ImageType::kImage3D)
              .set_image_size(128, 128, 128)
              .build());
      auto light_scattering_filtered = frame_graph_->add_resource(
          ImageTemplate("Light Scattering Filtered")
              .set_image_type(TextureType::kColor, VK_FORMAT_R16G16B16A16_SFLOAT,
                              ImageType::kImage3D)
              .set_image_size(128, 128, 128)
              .build());
      auto integrated_light_scattering = frame_graph_->add_resource(
          ImageTemplate("Integrated Light Scattering Texture")
              .set_image_type(TextureType::kColor, VK_FORMAT_R16G16B16A16_SFLOAT,
                              ImageType::kImage3D)
              .set_image_size(128, 128, 128)
              .build());
      auto volumetric_noise = frame_graph_->add_resource(
          ImageTemplate("Volumetric Noise Texture")
              .set_image_type(TextureType::kColor, VK_FORMAT_R8_UNORM, ImageType::kImage3D)
              .set_image_size(64, 64, 64)
              .set_initial_value({1.f, 0.f, 0.f, 1.f})
              .build());

      // todo update this
      auto post_process_sampler = repository_->get_sampler({
          .magFilter = VK_FILTER_LINEAR,
          .minFilter = VK_FILTER_LINEAR,
          .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
          .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
          .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
          .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
          .anisotropyEnable = VK_FALSE,
      });

      auto cube_map_sampler = repository_->material_buffers->cube_map_sampler;

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
      auto command_count_buffer
          = frame_graph_->add_resource(repository->mesh_buffers->command_count_buffer_instance);
      auto group_count_buffer
          = frame_graph_->add_resource(repository->mesh_buffers->group_count_buffer_instance);

      // Material
      
      auto brdf_lut = frame_graph_->add_resource(
          ImageTemplate("BRDF LUT Texture").set_initial_value(asset("bdrf_lut.png")).build(),
          fg::CreationType::EXTERNAL);
      auto texEnvMap = frame_graph_->add_resource(
          ImageTemplate("Environment Map Texture")
              .set_initial_value(asset("san_giuseppe_bridge_4k_environment.hdr"))
              .set_image_type(TextureType::kColor, VK_FORMAT_R32G32B32A32_SFLOAT, ImageType::kCubeMap)
              .build(),
          fg::CreationType::EXTERNAL);
      auto texIrradianceMap = frame_graph_->add_resource(
          ImageTemplate("Irradiance Map Texture")
              .set_initial_value(asset("san_giuseppe_bridge_4k_irradiance.hdr"))
              .set_image_type(TextureType::kColor, VK_FORMAT_R32G32B32A32_SFLOAT,
                              ImageType::kCubeMap)
              .build(),
          fg::CreationType::EXTERNAL);
          
      auto material_buffer
          = frame_graph_->add_resource(repository->material_buffers->material_buffer);

      auto material_textures
          = frame_graph_->add_resource(std::make_shared<ImageArrayInstance>(
                                           "PBR Textures",
                                           [this]() -> std::vector<Material> {
                                             return repository_->materials.data();
                                           },
                                           getMaxTextures()),
                                       fg::CreationType::EXTERNAL);

      // Light
      auto directional_light
          = frame_graph_->add_resource(repository->light_buffers->dir_light_buffer_instance);
      auto point_light
          = frame_graph_->add_resource(repository->light_buffers->point_light_buffer_instance);
      auto light_matrices
          = frame_graph_->add_resource(repository->light_buffers->view_proj_matrices_instance);

      // Shader Passes
      frame_graph_->add_pass<fg::DrawCullDirectionalDepthPass>(
          camera_buffer, meshlet_task_commands_buffer, mesh_draw_buffer, command_count_buffer, gpu_,
          [&]() { return static_cast<int32>(repository_->mesh_draws.size()); });

      frame_graph_->add_pass<fg::TaskSubmitDirectionalDepthPass>(
          meshlet_task_commands_buffer, command_count_buffer, group_count_buffer, gpu_);

      frame_graph_->add_pass<fg::MeshletDirectionalDepthPass>(
          camera_buffer, light_matrices, directional_light, point_light, vertex_position_buffer,
          vertex_data_buffer, meshlet_buffer, meshlet_vertices, meshlet_triangles,
          meshlet_task_commands_buffer, mesh_draw_buffer, group_count_buffer, shadow_map, gpu_);

      // mesh shading
      frame_graph_->add_pass<fg::DrawCullPass>(
          camera_buffer, meshlet_task_commands_buffer, mesh_draw_buffer, command_count_buffer, gpu_,
          [&] { return static_cast<int32>(repository_->mesh_draws.size()); });

      frame_graph_->add_pass<fg::TaskSubmitPass>(meshlet_task_commands_buffer, command_count_buffer,
                                                 group_count_buffer, gpu_);

      frame_graph_->add_pass<fg::MeshletPass>(
          camera_buffer, material_buffer, material_textures, vertex_position_buffer,
          vertex_data_buffer, meshlet_buffer, meshlet_vertices, meshlet_triangles,
          meshlet_task_commands_buffer, mesh_draw_buffer, group_count_buffer, g_buffer_1,
          g_buffer_2, g_buffer_3, g_buffer_depth, gpu_);

      frame_graph_->add_pass<fg::SsaoPass>(
          camera_buffer, g_buffer_depth, g_buffer_2, rotation_texture, ambient_occlusion_texture,
          post_process_sampler, gpu_, [&] { return config_.ssao; });

      frame_graph_->add_pass<fg::VolumetricLightingNoisePass>(volumetric_noise, gpu_);

      frame_graph_->add_pass<fg::VolumetricLightingInjectionPass>(
          blue_noise, volumetric_noise, froxel_data, post_process_sampler,
          [&] { return config_.volumetric_lighting; },
          [&] { return frame_->get_current_frame_number(); },
          [&] {
            return repository_->per_frame_data_buffers->data.at(frame_->get_current_frame_index());
          },
          gpu_);

      frame_graph_->add_pass<fg::VolumetricLightingScatteringPass>(
          camera_buffer, light_matrices, directional_light, point_light, blue_noise, froxel_data,
          shadow_map, light_scattering, post_process_sampler,
          [&] { return config_.volumetric_lighting; },
          [&] { return frame_->get_current_frame_number(); },
          [&] {
            return repository_->per_frame_data_buffers->data.at(frame_->get_current_frame_index());
          },
          [&] { return repository_->point_lights.size(); }, gpu_);

      frame_graph_->add_pass<fg::VolumetricLightingSpatialFilterPass>(
          light_scattering, light_scattering_filtered, post_process_sampler,
          [&] { return config_.volumetric_lighting; }, gpu_);

      frame_graph_->add_pass<fg::VolumetricLightingIntegrationPass>(
          light_scattering_filtered, integrated_light_scattering, post_process_sampler,
          [&] { return config_.volumetric_lighting; },
          [&] { return frame_->get_current_frame_number(); },
          [&] {
            return repository_->per_frame_data_buffers->data.at(frame_->get_current_frame_index());
          },
          gpu_);

      frame_graph_->add_pass<fg::LightingPass>(
          camera_buffer, texEnvMap, texIrradianceMap, brdf_lut, light_matrices, directional_light, point_light,
          g_buffer_1, g_buffer_2, g_buffer_3, g_buffer_depth, shadow_map,
          integrated_light_scattering, ambient_occlusion_texture, scene_lit, post_process_sampler, cube_map_sampler, gpu_, [&] { return config_.lighting; },
          [&] {
            return repository_->per_frame_data_buffers->data.at(frame_->get_current_frame_index());
          },
          [&] { return repository_->directional_lights.size(); },
          [&] { return repository_->point_lights.size(); });

      frame_graph_->add_pass<fg::SkyboxPass>(
          camera_buffer, directional_light, scene_lit, texEnvMap, scene_skybox, g_buffer_depth, post_process_sampler,
                                             gpu_, [&] { return config_.skybox; });

      frame_graph_->add_pass<fg::ToneMapPass>(scene_final, scene_lit, scene_skybox,
                                              post_process_sampler, gpu_,
                                              [&] { return config_.hdr; });

      frame_graph_->compile();
    }

    frames_.init(gpu_->getDevice(), gpu_->getGraphicsQueueFamily(), *frame_);
  }

  bool RenderEngine::acquire_next_image() {
    VK_CHECK(vkWaitForFences(gpu_->getDevice(), 1, &frame().render_fence, VK_TRUE, UINT64_MAX));
    vkDeviceWaitIdle(gpu_->getDevice()); // TODO fix synchronization issues!!!

    VK_CHECK(vkResetFences(gpu_->getDevice(), 1, &frame().render_fence));

    VkResult e
        = vkAcquireNextImageKHR(gpu_->getDevice(), swapchain_->swapchain, UINT64_MAX,
                                frame().swapchain_semaphore, nullptr, &swapchain_image_index_);
    if (e == VK_ERROR_OUT_OF_DATE_KHR) {
      resize_requested_ = true;
      return true;
    }
    VK_CHECK(e);

    VK_CHECK(vkResetCommandBuffer(frame().main_command_buffer, 0));

    return false;
  }

  CommandBuffer RenderEngine::start_draw() {
    const VkCommandBuffer cmd = frame().main_command_buffer;
    const VkCommandBufferBeginInfo cmd_begin_info
        = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));

    return CommandBuffer(cmd);
  }

  void RenderEngine::execute_passes() {
    if (resize_requested_) {
      swapchain_->resize_swapchain(window_);
      resize_requested_ = false;
    }

    if (acquire_next_image()) {
      return;
    }

    auto cmd = start_draw();

    resource_allocator_->flush();
    cmd.global_barrier();
    frame_graph_->execute(cmd);

    const auto swapchain_image = swapchain_->swapchain_images[swapchain_image_index_];

    {
      // Transition color_image to TRANSFER_SRC_OPTIMAL
      auto image_barrier = VkImageMemoryBarrier2{
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = scene_final_->get_current_stage(),
          .srcAccessMask = scene_final_->get_current_access(),
          .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
          .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
          .oldLayout = scene_final_->get_layout(),
          .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
          .image = scene_final_->get_image_handle(),
          .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
      };
      auto dependency_info = VkDependencyInfo{
          .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
          .imageMemoryBarrierCount = 1,
          .pImageMemoryBarriers = &image_barrier,
      };
      cmd.pipeline_barrier2(dependency_info);

      scene_final_->set_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
      scene_final_->set_current_access(VK_ACCESS_2_TRANSFER_READ_BIT);
      scene_final_->set_current_stage(VK_PIPELINE_STAGE_2_TRANSFER_BIT);
    }

    {
      // Transition swapchain_image to TRANSFER_DST_OPTIMAL
      auto image_barrier = VkImageMemoryBarrier2{
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
          .srcAccessMask = 0,
          .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
          .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
          .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          .image = swapchain_image->image,
          .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
      };
      auto dependency_info = VkDependencyInfo{
          .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
          .imageMemoryBarrierCount = 1,
          .pImageMemoryBarriers = &image_barrier,
      };
      cmd.pipeline_barrier2(dependency_info);
    }

    {
      // Insert a global barrier to ensure all transitions complete
      cmd.global_barrier();
    }

    {
      // Copy color_image to swapchain_image
      VkImageBlit2 blitRegion = {
          .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
          .pNext = nullptr,
          .srcSubresource = {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .mipLevel = 0,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
          .srcOffsets = {
              {0, 0, 0}, // srcOffset[0]
              {static_cast<int32_t>(scene_final_->get_extent().width), 
               static_cast<int32_t>(scene_final_->get_extent().height), 
               1}, // srcOffset[1]
          },
          .dstSubresource = {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .mipLevel = 0,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
          .dstOffsets = {
              {0, 0, 0}, // dstOffset[0]
              {static_cast<int32_t>(swapchain_image->getExtent2D().width), 
               static_cast<int32_t>(swapchain_image->getExtent2D().height), 
               1}, // dstOffset[1]
          },
      };

      // Blit the image
      VkBlitImageInfo2 blitInfo = {
          .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
          .pNext = nullptr,
          .srcImage = scene_final_->get_image_handle(),
          .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
          .dstImage = swapchain_image->image,
          .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          .regionCount = 1,
          .pRegions = &blitRegion,
          .filter = VK_FILTER_LINEAR,  // Use VK_FILTER_LINEAR or VK_FILTER_NEAREST
      };
      cmd.blit_image_2(blitInfo);
    }

    {
      // Transition swapchain_image back to COLOR_ATTACHMENT_OPTIMAL for ImGui rendering
      auto image_barrier = VkImageMemoryBarrier2{
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
          .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
          .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
          .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          .image = swapchain_image->image,
          .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
      };
      auto dependency_info = VkDependencyInfo{
          .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
          .imageMemoryBarrierCount = 1,
          .pImageMemoryBarriers = &image_barrier,
      };
      cmd.pipeline_barrier2(dependency_info);
    }

    {
      // Insert another global barrier to ensure everything is synchronized before presenting
      cmd.global_barrier();
    }

    // Render ImGui overlay
    imgui_->draw(cmd.get(), swapchain_image);

    {
      // Transition swapchain_image to PRESENT_SRC_KHR for presentation
      auto image_barrier = VkImageMemoryBarrier2{
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
          .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT ,
          .dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
          .dstAccessMask = 0,
          .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
          .image = swapchain_image->image,
          .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
      };
      auto dependency_info = VkDependencyInfo{
          .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
          .imageMemoryBarrierCount = 1,
          .pImageMemoryBarriers = &image_barrier,
      };
      cmd.pipeline_barrier2(dependency_info);
    }

    present(cmd);
  }

  void RenderEngine::cleanup() {
    frames_.cleanup(gpu_->getDevice());
    swapchain_->destroy_swapchain();
  }

  void RenderEngine::present(CommandBuffer cmd) {
    VK_CHECK(vkEndCommandBuffer(cmd.get()));

    VkCommandBufferSubmitInfo cmd_info = vkinit::command_buffer_submit_info(cmd.get());
    VkSemaphoreSubmitInfo wait_info = vkinit::semaphore_submit_info(
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, frame().swapchain_semaphore);
    VkSemaphoreSubmitInfo signal_info = vkinit::semaphore_submit_info(
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, frame().render_semaphore);

    const VkSubmitInfo2 submit = vkinit::submit_info(&cmd_info, &signal_info, &wait_info);

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