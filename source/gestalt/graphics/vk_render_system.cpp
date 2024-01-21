#include "vk_render_system.h"

#include <algorithm>
#include <chrono>

#include "imgui_gui.h"
#include "vk_images.h"
#include "vk_initializers.h"

void vk_render_system::draw(imgui_gui& imgui) {
  // wait until the gpu has finished rendering the last frame. Timeout of 1 second
  VK_CHECK(vkWaitForFences(gpu_.device, 1, &get_current_frame().render_fence, true, 1000000000));

  get_current_frame().deletion_queue.flush();
  get_current_frame().frame_descriptors.clear_pools(gpu_.device);
  // request image from the swapchain
  uint32_t swapchainImageIndex;

  VkResult e = vkAcquireNextImageKHR(gpu_.device, swapchain.swapchain, 1000000000,
                                     get_current_frame().swapchain_semaphore, nullptr,
                                     &swapchainImageIndex);
  if (e == VK_ERROR_OUT_OF_DATE_KHR) {
    resize_requested_ = true;
    return;
  }

  VK_CHECK(vkResetFences(gpu_.device, 1, &get_current_frame().render_fence));

  // now that we are sure that the commands finished executing, we can safely reset the command
  // buffer to begin recording again.
  VK_CHECK(vkResetCommandBuffer(get_current_frame().main_command_buffer, 0));

  // naming it cmd for shorter writing
  VkCommandBuffer cmd = get_current_frame().main_command_buffer;

  // begin the command buffer recording. We will use this command buffer exactly once, so we
  // want to let vulkan know that
  VkCommandBufferBeginInfo cmdBeginInfo
      = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  //> draw_first
  VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  // transition our main draw image into general layout so we can write into it
  // we will overwrite it all so we dont care about what was the older layout
  vkutil::transition_image(cmd, draw_image_.image, VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_GENERAL);
  vkutil::transition_image(cmd, depth_image_.image, VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  draw_main(cmd);

  // transtion the draw image and the swapchain image into their correct transfer layouts
  vkutil::transition_image(cmd, draw_image_.image, VK_IMAGE_LAYOUT_GENERAL,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  vkutil::transition_image(cmd, swapchain.swapchain_images[swapchainImageIndex],
                           VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  VkExtent2D extent;
  extent.height = window_.extent.height;
  extent.width = window_.extent.width;
  //< draw_first
  //> imgui_draw
  // execute a copy from the draw image into the swapchain
  vkutil::copy_image_to_image(cmd, draw_image_.image,
                              swapchain.swapchain_images[swapchainImageIndex], extent, extent);

  // set swapchain image layout to Attachment Optimal so we can draw it
  vkutil::transition_image(cmd, swapchain.swapchain_images[swapchainImageIndex],
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  // draw imgui into the swapchain image
  imgui.draw(cmd, swapchain.swapchain_image_views[swapchainImageIndex]);

  // set swapchain image layout to Present so we can draw it
  vkutil::transition_image(cmd, swapchain.swapchain_images[swapchainImageIndex],
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                           VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  // finalize the command buffer (we can no longer add commands, but it can now be executed)
  VK_CHECK(vkEndCommandBuffer(cmd));

  // prepare the submission to the queue.
  // we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain
  // is ready we will signal the _renderSemaphore, to signal that rendering has finished

  VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

  VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame().swapchain_semaphore);
  VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(
      VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame().render_semaphore);

  VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

  // submit command buffer to the queue and execute it.
  //  _renderFence will now block until the graphic commands finish execution
  VK_CHECK(vkQueueSubmit2(gpu_.graphics_queue, 1, &submit, get_current_frame().render_fence));

  // prepare present
  //  this will put the image we just rendered to into the visible window.
  //  we want to wait on the _renderSemaphore for that,
  //  as its necessary that drawing commands have finished before the image is displayed to the
  //  user
  VkPresentInfoKHR presentInfo = vkinit::present_info();

  presentInfo.pSwapchains = &swapchain.swapchain;
  presentInfo.swapchainCount = 1;

  presentInfo.pWaitSemaphores = &get_current_frame().render_semaphore;
  presentInfo.waitSemaphoreCount = 1;

  presentInfo.pImageIndices = &swapchainImageIndex;

  VkResult presentResult = vkQueuePresentKHR(gpu_.graphics_queue, &presentInfo);
  if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
    resize_requested_ = true;
    return;
  }
  // increase the number of frames drawn
  frame_number_++;
}

void vk_render_system::draw_main(VkCommandBuffer cmd) {
  compute_effect& effect = pipeline_manager.background_effects[0]; // todo hook this to GUI

  // bind the background compute pipeline
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

  // bind the descriptor set containing the draw image for the compute pipeline
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                          pipeline_manager.gradient_pipeline_layout, 0, 1,
                          &descriptor_manager.draw_image_descriptors, 0, nullptr);

  vkCmdPushConstants(cmd, pipeline_manager.gradient_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                     0, sizeof(compute_push_constants), &effect.data);
  // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide
  // by it
  vkCmdDispatch(cmd, std::ceil(window_.extent.width / 16.0),
                std::ceil(window_.extent.height / 16.0), 1);

  // draw the triangle

  VkRenderingAttachmentInfo colorAttachment
      = vkinit::attachment_info(draw_image_.imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
  VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(
      depth_image_.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  VkRenderingInfo renderInfo
      = vkinit::rendering_info(window_.extent, &colorAttachment, &depthAttachment);

  vkCmdBeginRendering(cmd, &renderInfo);
  auto start = std::chrono::system_clock::now();
  draw_geometry(cmd);

  auto end = std::chrono::system_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  stats_.mesh_draw_time = elapsed.count() / 1000.f;

  vkCmdEndRendering(cmd);
}

bool is_visible(const render_object& obj, const glm::mat4& viewproj) {
  std::array<glm::vec3, 8> corners{
      glm::vec3{1, 1, 1},  glm::vec3{1, 1, -1},  glm::vec3{1, -1, 1},  glm::vec3{1, -1, -1},
      glm::vec3{-1, 1, 1}, glm::vec3{-1, 1, -1}, glm::vec3{-1, -1, 1}, glm::vec3{-1, -1, -1},
  };

  glm::mat4 matrix = viewproj * obj.transform;

  glm::vec3 min = {1.5, 1.5, 1.5};
  glm::vec3 max = {-1.5, -1.5, -1.5};

  for (int c = 0; c < 8; c++) {
    // project each corner into clip space
    glm::vec4 v = matrix * glm::vec4(obj.bounds.origin + (corners[c] * obj.bounds.extents), 1.f);

    // perspective correction
    v.x = v.x / v.w;
    v.y = v.y / v.w;
    v.z = v.z / v.w;

    min = glm::min(glm::vec3{v.x, v.y, v.z}, min);
    max = glm::max(glm::vec3{v.x, v.y, v.z}, max);
  }

  // check the clip space box is within the view
  if (min.z > 1.f || max.z < 0.f || min.x > 1.f || max.x < -1.f || min.y > 1.f || max.y < -1.f) {
    return false;
  } else {
    return true;
  }
}

void vk_render_system::draw_geometry(VkCommandBuffer cmd) {
  // begin clock
  auto start = std::chrono::system_clock::now();

  std::vector<uint32_t> opaque_draws;
  opaque_draws.reserve(main_draw_context_.opaque_surfaces.size());

  for (size_t i = 0; i < main_draw_context_.opaque_surfaces.size(); i++) {
    //if (is_visible(main_draw_context_.opaque_surfaces[i], scene_data.viewproj)) {
      opaque_draws.push_back(i);
    //}
  }

  // sort the opaque surfaces by material and mesh
  std::ranges::sort(opaque_draws, [&](const auto& iA, const auto& iB) {
    const render_object& A = main_draw_context_.opaque_surfaces[iA];
    const render_object& B = main_draw_context_.opaque_surfaces[iB];
    if (A.material == B.material) {
      return A.index_buffer < B.index_buffer;
    }
    return A.material < B.material;
  });

  // allocate a new uniform buffer for the scene data
  AllocatedBuffer gpu_scene_data_buffer = resource_manager_.create_buffer(
      sizeof(gpu_scene_data), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

  // add it to the deletion queue of this frame so it gets deleted once its been used
  get_current_frame().deletion_queue.push_function([gpu_scene_data_buffer, this]() {
    vmaUnmapMemory(gpu_.allocator, gpu_scene_data_buffer.allocation);
    resource_manager_.destroy_buffer(gpu_scene_data_buffer);
  });

  void* mapped_data;
  VmaAllocation allocation = gpu_scene_data_buffer.allocation;
  VK_CHECK(vmaMapMemory(gpu_.allocator, allocation, &mapped_data));
  const auto scene_uniform_data = static_cast<gpu_scene_data*>(mapped_data);
  *scene_uniform_data = scene_data;

  // create a descriptor set that binds that buffer and update it
  VkDescriptorSet globalDescriptor = get_current_frame().frame_descriptors.allocate(
      gpu_.device, descriptor_manager.gpu_scene_data_descriptor_layout);

  DescriptorWriter writer;
  writer.write_buffer(0, gpu_scene_data_buffer.buffer, sizeof(gpu_scene_data), 0,
                      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  writer.update_set(gpu_.device, globalDescriptor);

  MaterialPipeline* lastPipeline = nullptr;
  MaterialInstance* lastMaterial = nullptr;
  VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

  auto draw = [&](const render_object& r) {
    if (r.material != lastMaterial) {
      lastMaterial = r.material;
      if (r.material->pipeline != lastPipeline) {
        lastPipeline = r.material->pipeline;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout,
                                0, 1, &globalDescriptor, 0, nullptr);

        VkViewport viewport = {};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = (float)window_.extent.width;
        viewport.height = (float)window_.extent.height;
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;

        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor = {};
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = window_.extent.width;
        scissor.extent.height = window_.extent.height;

        vkCmdSetScissor(cmd, 0, 1, &scissor);
      }

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 1,
                              1, &r.material->materialSet, 0, nullptr);
    }
    if (r.index_buffer != lastIndexBuffer) {
      lastIndexBuffer = r.index_buffer;
      vkCmdBindIndexBuffer(cmd, r.index_buffer, 0, VK_INDEX_TYPE_UINT32);
    }
    // calculate final mesh matrix
    GPUDrawPushConstants push_constants;
    push_constants.worldMatrix = r.transform;
    push_constants.vertexBuffer = r.vertex_buffer_address;

    vkCmdPushConstants(cmd, r.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(GPUDrawPushConstants), &push_constants);

    stats_.drawcall_count++;
    stats_.triangle_count += r.index_count / 3;
    vkCmdDrawIndexed(cmd, r.index_count, 1, r.first_index, 0, 0);
  };

  // reset counters
  stats_.drawcall_count = 0;
  stats_.triangle_count = 0;

  for (auto& r : opaque_draws) {
    draw(main_draw_context_.opaque_surfaces[r]);
  }

  for (auto& r : main_draw_context_.transparent_surfaces) {
    draw(r);
  }

  auto end = std::chrono::system_clock::now();

  // convert to microseconds (integer), and then come back to miliseconds
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  stats_.mesh_draw_time = elapsed.count() / 1000.f;

  // we delete the draw commands now that we processed them
  main_draw_context_.opaque_surfaces.clear();
  main_draw_context_.transparent_surfaces.clear();
}
