#include "vk_engine.h"

#if 0
#define VMA_DEBUG_INITIALIZE_ALLOCATIONS 1
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

#include <chrono>
#include <thread>
#include <utility>

render_engine* loaded_engine = nullptr;

constexpr bool use_validation_layers = true;

void render_engine::init() {

  assert(loaded_engine == nullptr);
  loaded_engine = this;

  window_.init({1000, 700}); // todo : get window size from config

  gpu_.init(use_validation_layers, window_,
            [this](auto func) { this->immediate_submit(std::move(func)); });

  resource_manager_->init(gpu_);

  frame_graph_->init(gpu_, window_, resource_manager_, imgui_);

  scene_manager_->init(gpu_, resource_manager_);

  register_gui_actions();
  imgui_->init(gpu_, window_, frame_graph_->get_swapchain(), gui_actions_);

  for (auto& cam : camera_positioners_) {
    auto free_fly_camera_ptr = std::make_unique<free_fly_camera>();
    free_fly_camera_ptr->init(glm::vec3(7, 1.8,-7), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
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
  gui_actions_.load_gltf = [this](std::string path) { scene_manager_->request_scene(std::move(path)); };
  gui_actions_.get_stats = [this]() -> engine_stats& { return stats_; };
  gui_actions_.get_scene_data = [this]() -> per_frame_data& { return resource_manager_->per_frame_data_; };
  
  gui_actions_.get_database = [this]() -> database& { return resource_manager_->get_database(); };
  gui_actions_.get_render_config = [this]() -> render_config& { return resource_manager_->config_; };
}

void render_engine::immediate_submit(std::function<void(VkCommandBuffer cmd)> function) {
    VK_CHECK(vkResetFences(gpu_.device, 1, &frame_graph_->get_sync().imgui_fence));
  VK_CHECK(vkResetCommandBuffer(frame_graph_->get_commands().imgui_command_buffer, 0));

    VkCommandBuffer cmd = frame_graph_->get_commands().imgui_command_buffer;

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
    VK_CHECK(vkQueueSubmit2(gpu_.graphics_queue, 1, &submit, frame_graph_->get_sync().imgui_fence));

    VK_CHECK(
        vkWaitForFences(gpu_.device, 1, &frame_graph_->get_sync().imgui_fence, true, 9999999999));
}

void render_engine::cleanup() {

    if (is_initialized_) {

      vkDeviceWaitIdle(gpu_.device);

      imgui_->cleanup();
      scene_manager_->cleanup();
      frame_graph_->cleanup();
      resource_manager_->cleanup();
      gpu_.cleanup();
      window_.cleanup();
    }
}

void render_engine::update_scene() {

  //TODO move to camera system

    glm::mat4 view = active_camera_.get_view_matrix();

    // camera projection
    glm::mat4 projection
        = glm::perspective(glm::radians(70.f),
                           (float)window_.extent.width / (float)window_.extent.height, .1f, 1000.f);
    
    // invert the Y direction on projection matrix so that we are more similar
    // to opengl and gltf axis
    projection[1][1] *= -1;

    auto& camera = resource_manager_->get_database().get_camera(0);
    camera.view_matrix = view;
    camera.projection_matrix = projection;

    resource_manager_ ->config_.light_adaptation.delta_time = time_tracking_service_.get_delta_time();

    scene_manager_->update_scene();
}

void render_engine::run()
{
    //begin clock
    auto start = std::chrono::system_clock::now(); // todo replace with timetracker

    time_tracking_service_.update_timer();

    SDL_Event e;

    while (!quit_) {
      while (SDL_PollEvent(&e) != 0) {
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

        imgui_->update(e);
      }

      active_camera_.update(time_tracking_service_.get_delta_time(), input_system_.get_movement());

      if (freeze_rendering_) {
        // throttle the speed to avoid the endless spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }

      imgui_->new_frame();

      update_scene();

      frame_graph_->execute_passes(); 
    }

    // get clock again, compare with start clock
    auto end = std::chrono::system_clock::now();

    // convert to microseconds (integer), and then come back to miliseconds
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    stats_.frametime = elapsed.count() / 1000.f;
}