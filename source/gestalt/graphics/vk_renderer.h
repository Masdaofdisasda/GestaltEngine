#pragma once

#include "imgui_gui.h"
#include "render_pass.h"
#include "resource_manager.h"
#include "vk_types.h"
#include "vk_gpu.h"
#include "scene_manager.h"

#include "vk_swapchain.h"
#include "vk_sync.h"

constexpr unsigned int FRAME_OVERLAP = 2;

class vk_renderer {
  vk_gpu gpu_ = {};
  sdl_window window_;
  std::shared_ptr<resource_manager> resource_manager_;
  std::shared_ptr<imgui_gui> imgui_;


  render_config config_;

  bool resize_requested_{false};
  uint32_t swapchain_image_index_{0};
  uint32_t frame_number_{0};
  std::vector<frame_data> frames_{FRAME_OVERLAP};

  std::unique_ptr<render_pass> skybox_pass_;
  std::unique_ptr<render_pass> geometry_pass_;
  std::unique_ptr<render_pass> transparency_pass_;
  std::unique_ptr<render_pass> ssao_pass_;
  std::unique_ptr<render_pass> hdr_pass_;

  bool acquire_next_image();
  VkCommandBuffer start_draw();
  void present(VkCommandBuffer cmd);

  std::shared_ptr<vk_swapchain> swapchain_ = std::make_unique<vk_swapchain>();
  std::unique_ptr<vk_command> commands_ = std::make_unique<vk_command>();
  std::unique_ptr<vk_sync> sync_ = std::make_unique<vk_sync>();

public:

  // draw resources
  double_buffered_frame_buffer frame_buffer_;
  draw_context main_draw_context_;
  engine_stats stats_;

  per_frame_data per_frame_data_;
  void init(const vk_gpu& gpu, const sdl_window& window,
            const std::shared_ptr<resource_manager>& resource_manager,
            const std::shared_ptr<imgui_gui>& imgui_gui, engine_stats stats);

  void draw();

  std::shared_ptr<vk_swapchain> get_swapchain() const { return swapchain_; }
  frame_data& get_current_frame() { return frames_[frame_number_ % FRAME_OVERLAP]; }
  vk_sync& get_sync() const { return *sync_; }
  vk_command& get_commands() const { return *commands_; }
  sdl_window& get_window() { return window_; }
  render_config& get_config() { return config_; }

  void cleanup() const {
    sync_->cleanup();
    commands_->cleanup();
    swapchain_->destroy_swapchain();
  }
};
