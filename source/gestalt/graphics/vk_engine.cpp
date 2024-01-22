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

#include <VkBootstrap.h>

#include <glm/gtx/transform.hpp>

#include <vk_initializers.h>
#include <vk_types.h>
#include <vk_images.h>
#include <vk_pipelines.h>

#include <chrono>
#include <thread>

render_engine* loaded_engine = nullptr;

constexpr bool use_validation_layers = true;

void render_engine::init() {
  // only one engine initialization is allowed with the application.
  assert(loaded_engine == nullptr);
  loaded_engine = this;

  window_.init({1920, 1080}); // todo : get window size from config

  gpu_.init(use_validation_layers, window_,
            [this](auto func) { this->immediate_submit(std::move(func)); });

  deletion_service_.init(gpu_.device, gpu_.allocator);

  resource_manager_.init(gpu_);

  renderer_.init(gpu_, window_, resource_manager_, resize_requested_, stats_);
  scene_manager_.init(gpu_, resource_manager_, renderer_.gltf_material);
  renderer_.scene_manager_ = &scene_manager_;

  register_gui_actions();
  imgui_.init(gpu_, window_, renderer_.swapchain, gui_actions_);


  renderer_.scene_data.ambientColor = glm::vec4(0.1f);
  renderer_.scene_data.sunlightColor = glm::vec4(1.f);
  renderer_.scene_data.sunlightDirection = glm::vec4(0.1, 0.5, 0.1, 10.f);

  for (auto& cam : camera_positioners_) {
    auto free_fly_camera_ptr = std::make_unique<free_fly_camera>();
    free_fly_camera_ptr->init(glm::vec3(0, 0,-15), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));
    cam = std::move(free_fly_camera_ptr);
  }
  active_camera_.init(*camera_positioners_.at(current_camera_positioner_index_));

  // everything went fine
  is_initialized_ = true;
}

void render_engine::register_gui_actions() {
  gui_actions_.exit = [this]() { quit_ = true; };
  gui_actions_.add_camera = [this]() {
    auto free_fly_camera_ptr = std::make_unique<free_fly_camera>();
    free_fly_camera_ptr->init(glm::vec3(0, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));
    camera_positioners_.push_back(std::move(free_fly_camera_ptr));
  };
  gui_actions_.get_stats = [this]() -> engine_stats& { return stats_; };
  gui_actions_.get_scene_data = [this]() -> gpu_scene_data& { return renderer_.scene_data; };
}

void render_engine::immediate_submit(std::function<void(VkCommandBuffer cmd)> function) {
    VK_CHECK(vkResetFences(gpu_.device, 1, &renderer_.sync.imgui_fence));
    VK_CHECK(vkResetCommandBuffer(renderer_.commands.imgui_command_buffer, 0));

    VkCommandBuffer cmd = renderer_.commands.imgui_command_buffer;

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
    VK_CHECK(vkQueueSubmit2(gpu_.graphics_queue, 1, &submit, renderer_.sync.imgui_fence));

    VK_CHECK(vkWaitForFences(gpu_.device, 1, &renderer_.sync.imgui_fence, true, 9999999999));
}

gpu_mesh_buffers render_engine::upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices) {
    //> mesh_create_1
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    gpu_mesh_buffers newSurface;

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

      deletion_service_.flush(); // needed?

      scene_manager_.cleanup();
      renderer_.cleanup();
      gpu_.cleanup();
      window_.cleanup();
    }
}

void render_engine::update_scene() {

    glm::mat4 view = active_camera_.get_view_matrix();

    // camera projection
    glm::mat4 projection
        = glm::perspective(glm::radians(70.f),
                           (float)window_.extent.width / (float)window_.extent.height, .1f, 1000.f);

    // invert the Y direction on projection matrix so that we are more similar
    // to opengl and gltf axis
    projection[1][1] *= -1;

    renderer_.scene_data.view = view;
    renderer_.scene_data.proj = projection;
    renderer_.scene_data.viewproj = projection * view;

    scene_manager_.update_scene(renderer_.main_draw_context_);
    //scene_manager_.loaded_scenes_["structure"]->Draw(glm::mat4{1.f}, renderer_.main_draw_context_);
}

void render_engine::run()
{
    //begin clock
    auto start = std::chrono::system_clock::now(); // todo replace with timetracker

    time_tracking_service_.update_timer();

    SDL_Event e;

    // main loop
    while (!quit_) {
      // Handle events on queue
      while (SDL_PollEvent(&e) != 0) {
        // close the window when user alt-f4s or clicks the X button
        if (e.type == SDL_QUIT) quit_ = true;

        input_system_.handle_event(e, window_.extent.width, window_.extent.height);

        if (e.type == SDL_WINDOWEVENT) {
          if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
            freeze_rendering_ = true;
          }
          if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
            freeze_rendering_ = false;
          }
        }

        // send SDL event to imgui for handling
        imgui_.update(e);
      }

      active_camera_.update(time_tracking_service_.get_delta_time(), input_system_.get_movement());

      if (freeze_rendering_) {
        // throttle the speed to avoid the endless spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }

      if (resize_requested_) {
        renderer_.swapchain.resize_swapchain(window_);
        resize_requested_ = false;
      }

      imgui_.new_frame(); // TODO

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
    writer.write_image(3, resources.normalImage.imageView, resources.normalSampler,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.write_image(4, resources.emissiveImage.imageView, resources.emissiveSampler,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.write_image(5, resources.occlusionImage.imageView, resources.occlusionSampler,
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