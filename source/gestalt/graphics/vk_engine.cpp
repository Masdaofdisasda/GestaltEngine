//> includes
#include "vk_engine.h"


#if 0
#define VMA_DEBUG_INITIALIZE_ALLOCATIONS 1
#  define VMA_DEBUG_LOG_FORMAT(format, ...)
#define VMA_DEBUG_LOG_FORMAT(format, ...) do { \
    printf((format), __VA_ARGS__); \
    printf("\n"); \
} while(false)
#endif
#define VMA_IMPLEMENTATION
#define VMA_VULKAN_VERSION 1003000
#include <vma/vk_mem_alloc.h>

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

#include <VkBootstrap.h>

#include <glm/gtx/transform.hpp>

#include <vk_initializers.h>
#include <vk_types.h>
#include <vk_images.h>
#include <vk_pipelines.h>

#include <chrono>
#include <thread>

render_engine* loaded_engine = nullptr;

constexpr bool bUseValidationLayers = true;

void render_engine::init() {
  // only one engine initialization is allowed with the application.
  assert(loaded_engine == nullptr);
  loaded_engine = this;

  window_.init({1920, 1080}); // todo : get window size from config

  gpu_.init(bUseValidationLayers, window_);

  deletion_service_.init(gpu_.device, gpu_.allocator);

  resource_manager_.init(gpu_.device, gpu_.allocator,
                         [this](auto func) { this->immediate_submit(std::move(func)); });

  // render system
  swapchain_.init(gpu_, window_, draw_image_, depth_image_);
  commands_.init(gpu_, frames_);
  sync_.init(gpu_, frames_);
  descriptor_manager_.init(gpu_, frames_, draw_image_);
  pipeline_manager_.init(gpu_, descriptor_manager_, metal_rough_material_, draw_image_, depth_image_);

  init_default_data();
  init_renderables();
  init_imgui();

  // everything went fine
  is_initialized_ = true;

  scene_data_.ambientColor = glm::vec4(0.1f);
  scene_data_.sunlightColor = glm::vec4(1.f);
  scene_data_.sunlightDirection = glm::vec4(0.1, 0.5, 0.1, 10.f);

  for (auto& cam : camera_positioners_) {
    auto free_fly_camera_ptr = std::make_unique<free_fly_camera>();
    free_fly_camera_ptr->init(glm::vec3(0, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));
    cam = std::move(free_fly_camera_ptr);
  }
  active_camera_.init(*camera_positioners_.at(current_camera_positioner_index_));

}

void render_engine::init_imgui() {
    // 1: create descriptor pool for IMGUI
    //  the size of the pool is very oversize, but it's copied from imgui demo
    //  itself.
    VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
                                         {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(gpu_.device, &pool_info, nullptr, &imguiPool));

    // 2: initialize imgui library

    // this initializes the core structures of imgui
    ImGui::CreateContext();

    // this initializes imgui for SDL
    ImGui_ImplSDL2_InitForVulkan(window_.handle);

    // this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = gpu_.instance;
    init_info.PhysicalDevice = gpu_.chosen_gpu;
    init_info.Device = gpu_.device;
    init_info.Queue = gpu_.graphics_queue;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;
    init_info.ColorAttachmentFormat = swapchain_.swapchain_image_format;

    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info, VK_NULL_HANDLE);

    // execute a gpu command to upload imgui font textures
    immediate_submit([&](VkCommandBuffer cmd) { ImGui_ImplVulkan_CreateFontsTexture(); });

    // clear font textures from cpu data
    ImGui_ImplVulkan_DestroyFontsTexture();

    // add the destroy the imgui created structures
    // NOTE: i think ImGui_ImplVulkan_Shutdown() destroy the imguiPool
    deletion_service_.push_function([this]() { ImGui_ImplVulkan_Shutdown(); });
}

void render_engine::init_default_data() {
    std::array<Vertex, 4> rect_vertices;

    rect_vertices[0].position = {0.5, -0.5, 0};
    rect_vertices[1].position = {0.5, 0.5, 0};
    rect_vertices[2].position = {-0.5, -0.5, 0};
    rect_vertices[3].position = {-0.5, 0.5, 0};

    rect_vertices[0].color = {0, 0, 0, 1};
    rect_vertices[1].color = {0.5, 0.5, 0.5, 1};
    rect_vertices[2].color = {1, 0, 0, 1};
    rect_vertices[3].color = {0, 1, 0, 1};

    rect_vertices[0].uv_x = 1;
    rect_vertices[0].uv_y = 0;
    rect_vertices[1].uv_x = 0;
    rect_vertices[1].uv_y = 0;
    rect_vertices[2].uv_x = 1;
    rect_vertices[2].uv_y = 1;
    rect_vertices[3].uv_x = 0;
    rect_vertices[3].uv_y = 1;

    std::array<uint32_t, 6> rect_indices;

    rect_indices[0] = 0;
    rect_indices[1] = 1;
    rect_indices[2] = 2;

    rect_indices[3] = 2;
    rect_indices[4] = 1;
    rect_indices[5] = 3;

    rectangle_ = upload_mesh(rect_indices, rect_vertices);

    // 3 default textures, white, grey, black. 1 pixel each
    uint32_t white = 0xFFFFFFFF;
    default_material_.white_image = resource_manager_.create_image(
        (void*)&white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
                               VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t grey = 0xAAAAAAFF;
    default_material_.grey_image = resource_manager_.create_image(
        (void*)&grey, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
                              VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t black = 0x000000FF;
    default_material_.black_image = resource_manager_.create_image(
        (void*)&black, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
                               VK_IMAGE_USAGE_SAMPLED_BIT);

    // checkerboard image
    uint32_t magenta = 0xFF00FFFF;
    constexpr size_t checkerboard_size = 256;
    std::array<uint32_t, checkerboard_size> pixels;  // for 16x16 checkerboard texture
    for (int x = 0; x < 16; x++) {
      for (int y = 0; y < 16; y++) {
        pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
      }
    }
    default_material_.error_checkerboard_image = resource_manager_.create_image(
        pixels.data(), VkExtent3D{16, 16, 1},
                                           VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

    sampl.magFilter = VK_FILTER_NEAREST;
    sampl.minFilter = VK_FILTER_NEAREST;

    vkCreateSampler(gpu_.device, &sampl, nullptr, &default_material_.default_sampler_nearest);

    sampl.magFilter = VK_FILTER_LINEAR;
    sampl.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(gpu_.device, &sampl, nullptr, &default_material_.default_sampler_linear);

    deletion_service_.push_function(
        [this]() { resource_manager_.destroy_image(default_material_.white_image); });
    deletion_service_.push_function(
        [this]() { resource_manager_.destroy_image(default_material_.grey_image); });
    deletion_service_.push_function(
        [this]() { resource_manager_.destroy_image(default_material_.error_checkerboard_image); });
    deletion_service_.push(default_material_.default_sampler_nearest);
    deletion_service_.push(default_material_.default_sampler_linear);
}

void render_engine::init_renderables() {
    std::string structurePath = {R"(..\..\assets\structure.glb)"};
    auto structureFile = loadGltf(this, structurePath);

    assert(structureFile.has_value());

    loaded_scenes_["structure"] = *structureFile;
}

void render_engine::immediate_submit(std::function<void(VkCommandBuffer cmd)> function) {
    VK_CHECK(vkResetFences(gpu_.device, 1, &sync_.imgui_fence));
    VK_CHECK(vkResetCommandBuffer(commands_.imgui_command_buffer, 0));

    VkCommandBuffer cmd = commands_.imgui_command_buffer;

    VkCommandBufferBeginInfo cmdBeginInfo
        = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    const auto moved_function = std::move(function);
    moved_function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, nullptr, nullptr);

    // submit command buffer to the queue and execute it.
    //  _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(gpu_.graphics_queue, 1, &submit, sync_.imgui_fence));

    VK_CHECK(vkWaitForFences(gpu_.device, 1, &sync_.imgui_fence, true, 9999999999));
}

GPUMeshBuffers render_engine::upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices) {
    //> mesh_create_1
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    GPUMeshBuffers newSurface;

    // create vertex buffer
    newSurface.vertexBuffer = resource_manager_.create_buffer(
        vertexBufferSize,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                        VMA_MEMORY_USAGE_GPU_ONLY);

    // find the adress of the vertex buffer
    VkBufferDeviceAddressInfo deviceAdressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = newSurface.vertexBuffer.buffer};
    newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(gpu_.device, &deviceAdressInfo);

    // create index buffer
    newSurface.indexBuffer = resource_manager_.create_buffer(
        indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    //< mesh_create_1
    //
    //> mesh_create_2
    AllocatedBuffer staging = resource_manager_.create_buffer(vertexBufferSize + indexBufferSize,
                                                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                        VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = staging.allocation->GetMappedData();

    // copy vertex buffer
    memcpy(data, vertices.data(), vertexBufferSize);
    // copy index buffer
    memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

    immediate_submit([&](VkCommandBuffer cmd) {
      VkBufferCopy vertexCopy{0};
      vertexCopy.dstOffset = 0;
      vertexCopy.srcOffset = 0;
      vertexCopy.size = vertexBufferSize;

      vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

      VkBufferCopy indexCopy{0};
      indexCopy.dstOffset = 0;
      indexCopy.srcOffset = vertexBufferSize;
      indexCopy.size = indexBufferSize;

      vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
    });

    resource_manager_.destroy_buffer(staging);

    return newSurface;
    //< mesh_create_2
}

void render_engine::cleanup() {

    if (is_initialized_) {

      vkDeviceWaitIdle(gpu_.device);

      loaded_scenes_.clear();


      for (frame_data frame : frames_) {
        frame.deletion_queue.flush();
      }

      deletion_service_.flush();

      descriptor_manager_.cleanup();
      sync_.cleanup();
      commands_.cleanup();
      swapchain_.destroy_swapchain();
      gpu_.cleanup();
      window_.cleanup();
    }
}

void render_engine::draw_main(VkCommandBuffer cmd) {

    compute_effect& effect = pipeline_manager_.background_effects[current_background_effect_];

    // bind the background compute pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

    // bind the descriptor set containing the draw image for the compute pipeline
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipeline_manager_.gradient_pipeline_layout, 0, 1,
                            &descriptor_manager_.draw_image_descriptors, 0, nullptr);

    vkCmdPushConstants(cmd, pipeline_manager_.gradient_pipeline_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(compute_push_constants), &effect.data);
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

void render_engine::draw() {
    {
      // wait until the gpu has finished rendering the last frame. Timeout of 1 second
      VK_CHECK(
          vkWaitForFences(gpu_.device, 1, &get_current_frame().render_fence, true, 1000000000));

      get_current_frame().deletion_queue.flush();
      get_current_frame().frame_descriptors.clear_pools(gpu_.device);
      // request image from the swapchain
      uint32_t swapchainImageIndex;

      VkResult e = vkAcquireNextImageKHR(gpu_.device, swapchain_.swapchain, 1000000000,
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
      vkutil::transition_image(cmd, swapchain_.swapchain_images[swapchainImageIndex],
                               VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

      VkExtent2D extent;
      extent.height = window_.extent.height;
      extent.width = window_.extent.width;
      //< draw_first
      //> imgui_draw
      // execute a copy from the draw image into the swapchain
      vkutil::copy_image_to_image(cmd, draw_image_.image,
                                  swapchain_.swapchain_images[swapchainImageIndex],
                                  extent, extent);

      // set swapchain image layout to Attachment Optimal so we can draw it
      vkutil::transition_image(cmd, swapchain_.swapchain_images[swapchainImageIndex],
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

      // draw imgui into the swapchain image
      draw_imgui(cmd, swapchain_.swapchain_image_views[swapchainImageIndex]);

      // set swapchain image layout to Present so we can draw it
      vkutil::transition_image(cmd, swapchain_.swapchain_images[swapchainImageIndex],
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

      // finalize the command buffer (we can no longer add commands, but it can now be executed)
      VK_CHECK(vkEndCommandBuffer(cmd));

      // prepare the submission to the queue.
      // we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain
      // is ready we will signal the _renderSemaphore, to signal that rendering has finished

      VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

      VkSemaphoreSubmitInfo waitInfo
          = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                          get_current_frame().swapchain_semaphore);
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

      presentInfo.pSwapchains = &swapchain_.swapchain;
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
}

void render_engine::draw_background(VkCommandBuffer cmd) {

    compute_effect& effect = pipeline_manager_.background_effects[current_background_effect_];

    // bind the background compute pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

    // bind the descriptor set containing the draw image for the compute pipeline
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipeline_manager_.gradient_pipeline_layout, 0, 1,
                            &descriptor_manager_.draw_image_descriptors, 0, nullptr);

    vkCmdPushConstants(cmd, pipeline_manager_.gradient_pipeline_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(compute_push_constants), &effect.data);
    // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide
    // by it
    vkCmdDispatch(cmd, std::ceil(swapchain_.draw_extent.width / 16.0),
                  std::ceil(swapchain_.draw_extent.height / 16.0),
                  1);
}

void render_engine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView) {
    VkRenderingAttachmentInfo colorAttachment
        = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
    VkRenderingInfo renderInfo
        = vkinit::rendering_info(swapchain_.swapchain_extent, &colorAttachment, nullptr);

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

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

void render_engine::draw_geometry(VkCommandBuffer cmd) {
    // begin clock
    auto start = std::chrono::system_clock::now();

    std::vector<uint32_t> opaque_draws;
    opaque_draws.reserve(main_draw_context_.opaque_surfaces.size());

    for (size_t i = 0; i < main_draw_context_.opaque_surfaces.size(); i++) {
      if (is_visible(main_draw_context_.opaque_surfaces[i], scene_data_.viewproj)) {
        opaque_draws.push_back(i);
      }
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
    AllocatedBuffer gpuSceneDataBuffer = resource_manager_.create_buffer(
        sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    // add it to the deletion queue of this frame so it gets deleted once its been used
    get_current_frame().deletion_queue.push_function(
        [gpuSceneDataBuffer, this]() { resource_manager_.destroy_buffer(gpuSceneDataBuffer); });

    // write the buffer
    GPUSceneData* sceneUniformData
        = (GPUSceneData*)gpuSceneDataBuffer.allocation->GetMappedData();
    *sceneUniformData = scene_data_;

    // create a descriptor set that binds that buffer and update it
    VkDescriptorSet globalDescriptor = get_current_frame().frame_descriptors.allocate(
        gpu_.device, descriptor_manager_.gpu_scene_data_descriptor_layout);

    DescriptorWriter writer;
    writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0,
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
          vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  r.material->pipeline->layout, 0, 1, &globalDescriptor, 0,
                                  nullptr);

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

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                r.material->pipeline->layout, 1, 1, &r.material->materialSet, 0,
                                nullptr);
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

void render_engine::update_scene() {

    glm::mat4 view = active_camera_.get_view_matrix();

    // camera projection
    glm::mat4 projection
        = glm::perspective(glm::radians(70.f),
                           (float)window_.extent.width / (float)window_.extent.height, 1.f, 1000.f);

    // invert the Y direction on projection matrix so that we are more similar
    // to opengl and gltf axis
    projection[1][1] *= -1;

    scene_data_.view = view;
    scene_data_.proj = projection;
    scene_data_.viewproj = projection * view;

    loaded_scenes_["structure"]->Draw(glm::mat4{1.f}, main_draw_context_);
}

void render_engine::run()
{
    //begin clock
    auto start = std::chrono::system_clock::now(); // todo replace with timetracker

    time_tracking_service_.update_timer();

    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
      // Handle events on queue
      while (SDL_PollEvent(&e) != 0) {
        // close the window when user alt-f4s or clicks the X button
        if (e.type == SDL_QUIT) bQuit = true;

        input_system_.handle_event(e, window_.extent.width, window_.extent.height);

        active_camera_.update(time_tracking_service_.get_delta_time(), input_system_.get_movement());

        if (e.type == SDL_WINDOWEVENT) {
          if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
            freeze_rendering_ = true;
          }
          if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
            freeze_rendering_ = false;
          }
        }

        // send SDL event to imgui for handling
        ImGui_ImplSDL2_ProcessEvent(&e);
      }

      if (freeze_rendering_) {
        // throttle the speed to avoid the endless spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }

      if (resize_requested_) {
        swapchain_.resize_swapchain(window_);
        resize_requested_ = false;
      }

      // imgui new frame
      ImGui_ImplVulkan_NewFrame();
      ImGui_ImplSDL2_NewFrame(window_.handle);
      ImGui::NewFrame();

      if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
          if (ImGui::MenuItem("Exit")) {
            bQuit = true;
          }
          // Add more menu items here if needed
          ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
          if (ImGui::BeginMenu("Add Camera")) {
            if (ImGui::MenuItem("Free Fly Camera")) {
              auto free_fly_camera_ptr = std::make_unique<free_fly_camera>();
              free_fly_camera_ptr->init(glm::vec3(0, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));
              camera_positioners_.push_back(std::move(free_fly_camera_ptr));
            }
            if (ImGui::MenuItem("Orbit Camera")) {
              // Code to add an Orbit Camera
            }
            ImGui::EndMenu();
          }

          if (ImGui::BeginMenu("Add Light Source")) {
            if (ImGui::MenuItem("Point Light")) {
              // Code to add an Orbit Camera
            }
            if (ImGui::MenuItem("Directional Light")) {
              // Code to add an Orbit Camera
            }
            if (ImGui::MenuItem("Spot Light")) {
              // Code to add an Orbit Camera
            }
            ImGui::EndMenu();
          }
          // Add more menu items here if needed
          ImGui::EndMenu();
        }
        // Add other menus like "Edit", "View", etc. here

        ImGui::EndMainMenuBar();
      }

      if (ImGui::Begin("Stats")) {
        ImGui::Text("frametime %f ms", stats_.frametime);
        ImGui::Text("draw time %f ms", stats_.mesh_draw_time);
        ImGui::Text("update time %f ms", stats_.scene_update_time);
        ImGui::Text("triangles %i", stats_.triangle_count);
        ImGui::Text("draws %i", stats_.drawcall_count);
        ImGui::End();
      }

      if (ImGui::Begin("Light")) {
        ImGui::SliderFloat("Light X", &scene_data_.sunlightDirection.x, -10.f, 10.f);
        ImGui::SliderFloat("Light Y", &scene_data_.sunlightDirection.y, -10.f, 10.f);
        ImGui::SliderFloat("Light Z", &scene_data_.sunlightDirection.z, -10.f, 10.f);
        ImGui::End();
      }

      if (ImGui::Begin("Cameras")) {
        for (size_t i = 0; i < camera_positioners_.size(); ++i) {
          std::string name = "Camera " + std::to_string(i);

          if (ImGui::Selectable(name.c_str(), current_camera_positioner_index_ == i)) {
            current_camera_positioner_index_ = i;
            active_camera_.set_positioner(camera_positioners_.at(i).get());
          }

          // If this is the currently selected camera, show input fields for position and rotation
          if (current_camera_positioner_index_ == i) {
            // Get the current position and orientation
            glm::vec3 position = camera_positioners_.at(i)->get_position();
            glm::quat orientation = camera_positioners_.at(i)->get_orientation();

            // Convert quaternion to Euler angles for user-friendly rotation input
            glm::vec3 rotation = glm::eulerAngles(orientation);

            // Display input fields for position
            if (ImGui::DragFloat3("Position", &position[0])) {
              camera_positioners_.at(i)->init(position, glm::vec3(0.f), glm::vec3(0, 1, 0));
            }
          }
        }
        ImGui::End();
      }



      if (ImGui::Begin("Background")) {

        ImGui::SliderFloat("Render Scale", &render_scale_, 0.3f, 1.f);

        compute_effect& selected
            = pipeline_manager_.background_effects[current_background_effect_];

        ImGui::Text("Selected effect: ", selected.name);

        ImGui::SliderInt("Effect Index", &current_background_effect_, 0,
                         pipeline_manager_.background_effects.size() - 1);

        ImGui::InputFloat4("data1", reinterpret_cast<float*>(&selected.data.data1));
        ImGui::InputFloat4("data2", reinterpret_cast<float*>(&selected.data.data2));
        ImGui::InputFloat4("data3", reinterpret_cast<float*>(&selected.data.data3));
        ImGui::InputFloat4("data4", reinterpret_cast<float*>(&selected.data.data4));

        ImGui::End();
      }

      ImGui::Render();

      update_scene();

      // our draw function
      draw();
    }

    // get clock again, compare with start clock
    auto end = std::chrono::system_clock::now();

    // convert to microseconds (integer), and then come back to miliseconds
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    stats_.frametime = elapsed.count() / 1000.f;
}

MaterialInstance gltf_metallic_roughness::write_material(
    VkDevice device, MaterialPass pass, const MaterialResources& resources,
    DescriptorAllocatorGrowable& descriptorAllocator) {
    MaterialInstance matData;
    matData.passType = pass;
    if (pass == MaterialPass::Transparent) {
      matData.pipeline = &transparentPipeline;
    } else {
      matData.pipeline = &opaquePipeline;
    }

    matData.materialSet = descriptorAllocator.allocate(device, materialLayout);

    writer.clear();
    writer.write_buffer(0, resources.dataBuffer, sizeof(MaterialConstants),
                        resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.write_image(1, resources.colorImage.imageView, resources.colorSampler,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.write_image(2, resources.metalRoughImage.imageView, resources.metalRoughSampler,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    writer.update_set(device, matData.materialSet);

    return matData;
}

void mesh_node::Draw(const glm::mat4& topMatrix, draw_context& ctx) {
    glm::mat4 nodeMatrix = topMatrix * worldTransform;

    for (auto& s : mesh->surfaces) {
      render_object def;
      def.index_count = s.count;
      def.first_index = s.startIndex;
      def.index_buffer = mesh->meshBuffers.indexBuffer.buffer;
      def.material = &s.material->data;
      def.bounds = s.bounds;
      def.transform = nodeMatrix;
      def.vertex_buffer_address = mesh->meshBuffers.vertexBufferAddress;

      if (s.material->data.passType == MaterialPass::Transparent) {
        ctx.transparent_surfaces.push_back(def);
      } else {
        ctx.opaque_surfaces.push_back(def);
      }
    }

    // recurse down
    Node::Draw(topMatrix, ctx);
}