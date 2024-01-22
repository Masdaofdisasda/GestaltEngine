#pragma once

#include "vk_types.h"
#include "vk_deletion_service.h"
#include "vk_gpu.h"
#include "vk_swapchain.h"
#include "gui_actions.h"

class imgui_gui {
  vk_gpu gpu_;
  sdl_window window_;
  vk_swapchain swapchain_;
  gui_actions actions_;
  vk_deletion_service deletion_service_;


public:

  void init(vk_gpu& gpu, sdl_window& window, vk_swapchain& swapchain, gui_actions& actions);

  void cleanup();

  void draw(VkCommandBuffer cmd, VkImageView target_image_view);

  void update(const SDL_Event& e);

  void new_frame();
};
