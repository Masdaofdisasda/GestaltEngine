#include "Engine.hpp"

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

#include <tracy/Tracy.hpp>

#include "vk_initializers.hpp"
#include "vk_types.hpp"

#include <chrono>
#include <thread>
#include <utility>

namespace gestalt {

  Engine* loaded_engine = nullptr;

  constexpr bool kUseValidationLayers = true;

  void Engine::init() {
    assert(loaded_engine == nullptr);
    loaded_engine = this;

    window_.init({1300, 900});  // todo : get window size from config

    auto gpu = graphics::Gpu{};
    gpu.init(kUseValidationLayers, window_,
             [this](auto func) { this->immediate_submit(std::move(func)); });
    gpu_ = std::make_shared<graphics::Gpu>(gpu);

    resource_manager_->init(gpu_, repository_);

    scene_manager_->init(
        gpu_, resource_manager_,
                         std::make_unique<graphics::DescriptorUtilFactory>(), repository_);

    frame_graph_->init(gpu_, window_, resource_manager_, repository_, imgui_);

    register_gui_actions();
    imgui_->init(
        gpu_, window_, frame_graph_->get_swapchain_format(), repository_,
                 std::make_unique<graphics::DescriptorUtilFactory>(),
        gui_actions_);

    // everything went fine
    is_initialized_ = true;
  }

  void Engine::register_gui_actions() {
    gui_actions_.exit = [this]() { quit_ = true; };
    gui_actions_.load_gltf
        = [this](std::string path) { scene_manager_->request_scene(std::move(path)); };
    gui_actions_.get_stats = [this]() -> foundation::EngineStats& { return stats_; };
    gui_actions_.get_component_factory = [this]() -> application::ComponentFactory& {
      return scene_manager_->get_component_factory();
    };
    gui_actions_.get_render_config
        = [this]() -> graphics::RenderConfig& { return frame_graph_->get_config(); };
    gui_actions_.get_debug_image = [this]() -> std::shared_ptr<foundation::TextureHandle> {
           return frame_graph_->get_debug_image();
    };
  }

  void Engine::immediate_submit(std::function<void(VkCommandBuffer cmd)> function) {
    VK_CHECK(vkResetFences(gpu_->getDevice(), 1, &frame_graph_->get_sync().imgui_fence));
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
    VK_CHECK(vkQueueSubmit2(gpu_->getGraphicsQueue(), 1, &submit, frame_graph_->get_sync().imgui_fence));

    VK_CHECK(
        vkWaitForFences(gpu_->getDevice(), 1, &frame_graph_->get_sync().imgui_fence, true, 9999999999));
  }

  void Engine::cleanup() const {
    if (is_initialized_) {
      vkDeviceWaitIdle(gpu_->getDevice());

      imgui_->cleanup();
      scene_manager_->cleanup();
      frame_graph_->cleanup();
      resource_manager_->cleanup();
      gpu_->cleanup();
      window_.cleanup();
    }
  }

  void Engine::update_scene() const {

    frame_graph_->get_config().light_adaptation.delta_time
        = time_tracking_service_.get_delta_time();

    scene_manager_->update_scene(time_tracking_service_.get_delta_time(),
                                 input_system_.get_movement(),
                                 static_cast<float>(window_.extent.width) / static_cast<float>(window_.extent.height));
  }

  void Engine::run() {
    // begin clock
    auto start = std::chrono::system_clock::now();  // todo replace with timetracker

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

      if (freeze_rendering_) {
        // throttle the speed to avoid the endless spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }

      imgui_->new_frame();

      update_scene();

      frame_graph_->execute_passes();

      graphics::gpu_frame.next_frame();

      FrameMark;
    }

    // get clock again, compare with start clock
    auto end = std::chrono::system_clock::now();

    // convert to microseconds (integer), and then come back to miliseconds
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    stats_.frametime = elapsed.count() / 1000.f;
  }
}  // namespace gestalt