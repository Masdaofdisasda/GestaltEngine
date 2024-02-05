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
  bool resize_requested_{false};

  std::unique_ptr<render_pass> skybox_pass_;
  std::unique_ptr<render_pass> geometry_pass_;
  std::unique_ptr<render_pass> transparency_pass;

public:
  PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR;
  vk_gpu gpu_ = {};
  sdl_window window_;
  std::shared_ptr<resource_manager> resource_manager_;
  std::shared_ptr<scene_manager> scene_manager_;
  std::shared_ptr<imgui_gui> imgui_;

  vk_swapchain swapchain;
  vk_command commands;
  vk_sync sync;

  int frame_number_{0};
  std::vector<frame_data> frames_{FRAME_OVERLAP};
  frame_data& get_current_frame() { return frames_[frame_number_ % FRAME_OVERLAP]; }

  // draw resources
  frame_buffer frame_buffer_;

  draw_context main_draw_context_;

  engine_stats stats_;

  per_frame_data per_frame_data_;
  void init(const vk_gpu& gpu, const sdl_window& window,
            const std::shared_ptr<resource_manager>& resource_manager,
            const std::shared_ptr<scene_manager>& scene_manager,
            const std::shared_ptr<imgui_gui>& imgui_gui, const bool& resize_requested,
            engine_stats stats);

  void draw();

  void cleanup() {

    sync.cleanup();
    commands.cleanup();
    swapchain.destroy_swapchain();
  }
};
