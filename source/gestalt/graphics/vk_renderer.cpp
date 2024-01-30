#include "vk_renderer.h"

#include <algorithm>
#include <chrono>

#include "imgui_gui.h"
#include "vk_images.h"
#include "vk_initializers.h"

void skybox_pass::init(vk_renderer& renderer) {
  gpu_ = renderer.gpu_;
  renderer_ = &renderer;

  std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
  };

  descriptor_allocator_.init(gpu_.device, 3, sizes);

  {
    descriptor_layout_
        = descriptor_layout_builder()
              .add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
              .add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_FRAGMENT_BIT)
              .build(gpu_.device);
  }

  VkPipelineLayoutCreateInfo pipeline_layout_create_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .setLayoutCount = 1,
      .pSetLayouts = &descriptor_layout_,
  };

  VK_CHECK(vkCreatePipelineLayout(gpu_.device, &pipeline_layout_create_info, nullptr,
                                  &pipeline_layout_));

  VkShaderModule vertex_shader;
  vkutil::load_shader_module(vertex_shader_source_.c_str(), gpu_.device, &vertex_shader);
  VkShaderModule fragment_shader;
  vkutil::load_shader_module(fragment_shader_source_.c_str(), gpu_.device, &fragment_shader);

  pipeline_ = PipelineBuilder()
                  .set_shaders(vertex_shader, fragment_shader)
                  .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                  .set_polygon_mode(VK_POLYGON_MODE_FILL)
                  .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                  .set_multisampling_none()
                  .disable_blending()
                  .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
                  .set_color_attachment_format(renderer_->frame_buffer_.color_image.imageFormat)
                  .set_depth_format(renderer_->frame_buffer_.depth_image.imageFormat)
                  .set_pipeline_layout(pipeline_layout_)
                  .build_pipeline(gpu_.device);

  // Clean up shader modules after pipeline creation
  vkDestroyShaderModule(gpu_.device, vertex_shader, nullptr);
  vkDestroyShaderModule(gpu_.device, fragment_shader, nullptr);

  VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
  sampl.maxLod = VK_LOD_CLAMP_NONE;
  sampl.minLod = 0;
  sampl.magFilter = VK_FILTER_LINEAR;
  sampl.minFilter = VK_FILTER_LINEAR;
  sampl.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  vkCreateSampler(gpu_.device, &sampl, nullptr, &cube_map_sampler);

  uint32_t white = 0xFFFFFFFF;    // White
  uint32_t red = 0xFFFF0000;      // Red
  uint32_t green = 0xFF00FF00;    // Green
  uint32_t blue = 0xFF0000FF;     // Blue
  uint32_t yellow = 0xFFFFFF00;   // Yellow
  uint32_t magenta = 0xFFFF00FF;  // Magenta

  // Prepare array with one color for each face of the cube
  std::array<void*, 6> cube_colors = {&white, &red, &green, &blue, &yellow, &magenta};

  //cube_map_image = renderer_->resource_manager_->create_cubemap(
      //cube_colors, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
}

void skybox_pass::cleanup() {
  descriptor_allocator_.destroy_pools(gpu_.device);

  vkDestroyPipelineLayout(gpu_.device, pipeline_layout_, nullptr);
  vkDestroyPipeline(gpu_.device, pipeline_, nullptr);

  vkDestroyDescriptorSetLayout(gpu_.device, descriptor_layout_, nullptr);
}

void skybox_pass::bind_resources() {
  descriptor_set_ = descriptor_allocator_.allocate(gpu_.device, descriptor_layout_);

  writer.clear();
  writer.write_image(1, renderer_->resource_manager_->filtered_map.imageView, cube_map_sampler, VK_IMAGE_LAYOUT_GENERAL,
                     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
}

void skybox_pass::execute(const VkCommandBuffer cmd) {
  writer.update_set(gpu_.device, descriptor_set_);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                          &descriptor_set_, 0, nullptr);
  VkViewport viewport = {};
  viewport.x = 0;
  viewport.y = 0;
  viewport.width = static_cast<float>(renderer_->window_.extent.width);
  viewport.height = static_cast<float>(renderer_->window_.extent.height);
  viewport.minDepth = 0.f;
  viewport.maxDepth = 1.f;

  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = {};
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent.width = renderer_->window_.extent.width;
  scissor.extent.height = renderer_->window_.extent.height;

  vkCmdSetScissor(cmd, 0, 1, &scissor);
  vkCmdDraw(cmd, 36, 1, 0, 0);  // 36 vertices for the cube
}

void pbr_pass::init(vk_renderer& renderer) {
  gpu_ = renderer.gpu_;
  renderer_ = &renderer;

  {
    descriptor_layout_ = descriptor_layout_builder()
                             .add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                             .build(gpu_.device);
  }

  for (auto& frame : renderer_->frames_) {
    // create a descriptor pool
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
    };

    frame.descriptor_pools = DescriptorAllocatorGrowable{};
    frame.descriptor_pools.init(gpu_.device, 1000, frame_sizes);
  }

  VkShaderModule meshFragShader;
  vkutil::load_shader_module(fragment_shader_source_.c_str(), gpu_.device, &meshFragShader);

  VkShaderModule meshVertexShader;
  vkutil::load_shader_module(vertex_shader_source_.c_str(), gpu_.device, &meshVertexShader);

  VkPushConstantRange matrix_range{};
  matrix_range.offset = 0;
  matrix_range.size = sizeof(GPUDrawPushConstants);
  matrix_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkDescriptorSetLayout layouts[]
      = {descriptor_layout_, renderer_->resource_manager_->materialLayout,
         renderer_->resource_manager_->materialConstantsLayout};

  VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
  mesh_layout_info.setLayoutCount = 3;
  mesh_layout_info.pSetLayouts = layouts;
  mesh_layout_info.pPushConstantRanges = &matrix_range;
  mesh_layout_info.pushConstantRangeCount = 1;

  VkPipelineLayout newLayout;
  VK_CHECK(vkCreatePipelineLayout(gpu_.device, &mesh_layout_info, nullptr, &newLayout));

  PipelineBuilder pipelineBuilder;
  opaquePipelineLayout = newLayout;
  transparentPipelineLayout = newLayout;

  opaquePipeline
      = pipelineBuilder.set_shaders(meshVertexShader, meshFragShader)
            .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .set_polygon_mode(VK_POLYGON_MODE_FILL)
            .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .set_multisampling_none()
            .disable_blending()
            .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
            .set_color_attachment_format(renderer_->frame_buffer_.color_image.imageFormat)
            .set_depth_format(renderer_->frame_buffer_.depth_image.imageFormat)
            .set_pipeline_layout(newLayout)
            .build_pipeline(gpu_.device);

  // create the transparent variant
  transparentPipeline = pipelineBuilder.enable_blending_additive()
                            .enable_depthtest(false, VK_COMPARE_OP_LESS_OR_EQUAL)
                            .build_pipeline(gpu_.device);

  vkDestroyShaderModule(gpu_.device, meshFragShader, nullptr);
  vkDestroyShaderModule(gpu_.device, meshVertexShader, nullptr);
}

void pbr_pass::cleanup() {
  vkDestroyPipeline(gpu_.device, transparentPipeline, nullptr);
  vkDestroyPipeline(gpu_.device, opaquePipeline, nullptr);

  for (auto frame : renderer_->frames_) {
    frame.descriptor_pools.destroy_pools(gpu_.device);
  }
  vkDestroyDescriptorSetLayout(gpu_.device, descriptor_layout_, nullptr);
}

void pbr_pass::bind_resources() {
    descriptor_set_ = renderer_->get_current_frame().descriptor_pools.allocate(
        gpu_.device, descriptor_layout_);

  writer.clear();
  writer.write_buffer(0, renderer_->gpu_scene_data_buffer.buffer, sizeof(gpu_scene_data), 0,
      					  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

void pbr_pass::execute(VkCommandBuffer cmd) {
  std::vector<uint32_t> opaque_draws;
  opaque_draws.reserve(renderer_->main_draw_context_.opaque_surfaces.size());

  for (size_t i = 0; i < renderer_->main_draw_context_.opaque_surfaces.size(); i++) {
    //if (is_visible(main_draw_context_.opaque_surfaces[i], scene_data.viewproj)) {
      opaque_draws.push_back(i);
    //}
  }

  // sort the opaque surfaces by material and mesh
  std::ranges::sort(opaque_draws, [&](const auto& iA, const auto& iB) {
    const render_object& A = renderer_->main_draw_context_.opaque_surfaces[iA];
    const render_object& B = renderer_->main_draw_context_.opaque_surfaces[iB];
    if (A.material == B.material) {
      return A.first_index < B.first_index;
    }
    return A.material < B.material;
  });

  bind_resources();
  writer.update_set(gpu_.device, descriptor_set_);

  vkCmdBindIndexBuffer(cmd, renderer_->resource_manager_->scene_geometry_.indexBuffer.buffer, 0,
                       VK_INDEX_TYPE_UINT32);

  auto draw = [&](const render_object& r, VkPipelineLayout pipeline_layout) {

    GPUDrawPushConstants push_constants;
    push_constants.worldMatrix = r.transform;
    push_constants.material_id = r.material * 5;
    push_constants.vertexBuffer = r.vertex_buffer_address;
    vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(GPUDrawPushConstants), &push_constants);

    renderer_->stats_.drawcall_count++;
    renderer_->stats_.triangle_count += r.index_count / 3;
    vkCmdDrawIndexed(cmd, r.index_count, 1, r.first_index, 0, 0);
  };

  // reset counters
  renderer_->stats_.drawcall_count = 0;
  renderer_->stats_.triangle_count = 0;
  
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, opaquePipeline);
  VkDescriptorSet descriptorSets[] = {descriptor_set_, renderer_->resource_manager_->materialSet,
                                      renderer_->resource_manager_->materialConstantsSet};
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, opaquePipelineLayout, 0, 3,
                          descriptorSets, 0, nullptr);

  const VkViewport viewport = {
       .x = 0,
      .y = 0,
      .width = static_cast<float>(renderer_->window_.extent.width),
      .height = static_cast<float>(renderer_->window_.extent.height),
      .minDepth = 0.f,
      .maxDepth = 1.f,
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = {};
  scissor.offset.y = 0;
  scissor.offset.x = 0;
  scissor.extent.width = renderer_->window_.extent.width;
  scissor.extent.height = renderer_->window_.extent.height;
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  for (auto& r : opaque_draws) {
      draw(renderer_->main_draw_context_.opaque_surfaces[r], opaquePipelineLayout);
  }

  
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, transparentPipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, transparentPipelineLayout, 0, 3,
                          descriptorSets, 0, nullptr);

  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  for (auto& r : renderer_->main_draw_context_.transparent_surfaces) {
    draw(r, transparentPipelineLayout);
  }
}

void vk_renderer::init(const vk_gpu& gpu, const sdl_window& window,
                       resource_manager* resource_manager,
                       const bool& resize_requested, engine_stats stats) {
  gpu_ = gpu;
  window_ = window;
  resource_manager_ = resource_manager;
  resize_requested_ = resize_requested;
  stats_ = stats;

  swapchain.init(gpu_, window_, frame_buffer_);
  commands.init(gpu_, frames_);
  sync.init(gpu_, frames_);

  gpu_scene_data_buffer = resource_manager_->create_buffer(
      sizeof(gpu_scene_data), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  skybox_pass_.init(*this);
  pbr_pass_.init(*this);
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
  // wait until the gpu has finished rendering the last frame. Timeout of 1 second
  VK_CHECK(vkWaitForFences(gpu_.device, 1, &get_current_frame().render_fence, true, 1000000000));

  get_current_frame().descriptor_pools.clear_pools(gpu_.device);
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
  VmaAllocation allocation = gpu_scene_data_buffer.allocation;
  VK_CHECK(vmaMapMemory(gpu_.allocator, allocation, &mapped_data));
  const auto scene_uniform_data = static_cast<gpu_scene_data*>(mapped_data);
  *scene_uniform_data = scene_data;

  skybox_pass_.bind_resources();
  skybox_pass_.writer.write_buffer(0, gpu_scene_data_buffer.buffer, sizeof(gpu_scene_data), 0,
                                   VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  skybox_pass_.execute(cmd);

  auto start = std::chrono::system_clock::now();
  {
    // begin clock
    auto start = std::chrono::system_clock::now();

    pbr_pass_.execute(cmd);

    auto end = std::chrono::system_clock::now();

    // convert to microseconds (integer), and then come back to miliseconds
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    stats_.mesh_draw_time = elapsed.count() / 1000.f;

    // we delete the draw commands now that we processed them
    main_draw_context_.opaque_surfaces.clear();
    main_draw_context_.transparent_surfaces.clear();
  }

  vmaUnmapMemory(gpu_.allocator, gpu_scene_data_buffer.allocation);

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
