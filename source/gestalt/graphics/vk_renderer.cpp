#include "vk_renderer.h"

#include <chrono>

#include "imgui_gui.h"
#include "geometry_pass.h"
#include "vk_images.h"
#include "vk_initializers.h"
#include "skybox_pass.h"
#include "transparent_pass.h"

void vk_renderer::init(const vk_gpu& gpu, const sdl_window& window,
                       const std::shared_ptr<resource_manager>& resource_manager,
                       const std::shared_ptr<scene_manager>& scene_manager,
                       const std::shared_ptr<imgui_gui>& imgui_gui, const bool& resize_requested,
                       engine_stats stats) {
  gpu_ = gpu;
  window_ = window;
  resource_manager_ = resource_manager;
  scene_manager_ = scene_manager;
  imgui_ = imgui_gui;
  resize_requested_ = resize_requested;
  stats_ = stats;

  swapchain.init(gpu_, window_, frame_buffer_);
  commands.init(gpu_, frames_);
  sync.init(gpu_, frames_);

  skybox_pass_ = std::make_unique<skybox_pass>();
  skybox_pass_->init(*this);
  geometry_pass_ = std::make_unique<geometry_pass>();
  geometry_pass_->init(*this);
  transparency_pass = std::make_unique<transparent_pass>();
  transparency_pass->init(*this);
  
  per_frame_data_.ambientColor = glm::vec4(0.1f);
  per_frame_data_.sunlightColor = glm::vec4(1.f);
  per_frame_data_.sunlightDirection = glm::vec4(0.1, 0.5, 0.1, 1.5f);

  vkCmdPushDescriptorSetKHR = reinterpret_cast<
    PFN_vkCmdPushDescriptorSetKHR>(
    vkGetDeviceProcAddr(gpu_.device, "vkCmdPushDescriptorSetKHR"));

  if (!vkCmdPushDescriptorSetKHR) {
    throw std::runtime_error("Failed to load vkCmdPushDescriptorSetKHR");
  }
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
  }

  return true;
}

void vk_renderer::draw() {

  VK_CHECK(vkWaitForFences(gpu_.device, 1, &get_current_frame().render_fence, true, 1000000000));

  get_current_frame().descriptor_pools.clear_pools(gpu_.device);
  uint32_t swapchainImageIndex;

  VkResult e = vkAcquireNextImageKHR(gpu_.device, swapchain.swapchain, 1000000000,
                                     get_current_frame().swapchain_semaphore, nullptr,
                                     &swapchainImageIndex);
  if (e == VK_ERROR_OUT_OF_DATE_KHR) {
    resize_requested_ = true;
    return;
  }

  VK_CHECK(vkResetFences(gpu_.device, 1, &get_current_frame().render_fence));
  VK_CHECK(vkResetCommandBuffer(get_current_frame().main_command_buffer, 0));

  VkCommandBuffer cmd = get_current_frame().main_command_buffer;
  VkCommandBufferBeginInfo cmdBeginInfo
      = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);


  VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  vkutil::transition_image(cmd, frame_buffer_.color_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_GENERAL);
  vkutil::transition_image(cmd, frame_buffer_.depth_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  // DRAW MAIN
  VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(
      frame_buffer_.color_image.imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
  VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(
      frame_buffer_.depth_image.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  VkRenderingInfo renderInfo
      = vkinit::rendering_info(window_.extent, &colorAttachment, &depthAttachment);

  vkCmdBeginRendering(cmd, &renderInfo);

  void* mapped_data;
  VmaAllocation allocation = resource_manager_->per_frame_data_buffer.allocation;
  VK_CHECK(vmaMapMemory(gpu_.allocator, allocation, &mapped_data));
  const auto scene_uniform_data = static_cast<per_frame_data*>(mapped_data);
  *scene_uniform_data = per_frame_data_;

  skybox_pass_->execute(cmd);

  auto start = std::chrono::system_clock::now();
  {
    // begin clock
    auto start = std::chrono::system_clock::now();

    geometry_pass_->execute(cmd);
    transparency_pass->execute(cmd);

    auto end = std::chrono::system_clock::now();

    // convert to microseconds (integer), and then come back to miliseconds
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    stats_.mesh_draw_time = elapsed.count() / 1000.f;

    // we delete the draw commands now that we processed them
    main_draw_context_.opaque_surfaces.clear();
    main_draw_context_.transparent_surfaces.clear();
  }

  vmaUnmapMemory(gpu_.allocator, resource_manager_->per_frame_data_buffer.allocation);

  auto end = std::chrono::system_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  stats_.mesh_draw_time = elapsed.count() / 1000.f;

  vkCmdEndRendering(cmd);
  // END DRAW MAIN

  vkutil::transition_image(cmd, frame_buffer_.color_image.image, VK_IMAGE_LAYOUT_GENERAL,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  vkutil::transition_image(cmd, swapchain.swapchain_images[swapchainImageIndex],
                           VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  VkExtent2D extent;
  extent.height = window_.extent.height;
  extent.width = window_.extent.width;

  vkutil::copy_image_to_image(cmd, frame_buffer_.color_image.image,
                              swapchain.swapchain_images[swapchainImageIndex], extent, extent);
  vkutil::transition_image(cmd, swapchain.swapchain_images[swapchainImageIndex],
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  imgui_->draw(cmd, swapchain.swapchain_image_views[swapchainImageIndex]);

  vkutil::transition_image(cmd, swapchain.swapchain_images[swapchainImageIndex],
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                           VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  VK_CHECK(vkEndCommandBuffer(cmd));

  VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
  VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame().swapchain_semaphore);
  VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(
      VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame().render_semaphore);

  VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

  VK_CHECK(vkQueueSubmit2(gpu_.graphics_queue, 1, &submit, get_current_frame().render_fence));
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

  frame_number_++;
}
