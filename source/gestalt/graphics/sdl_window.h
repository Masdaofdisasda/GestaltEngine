#pragma once

#include <include/SDL.h>
#include <include/SDL_vulkan.h>
#include "vk_types.h"

class sdl_window {

public:
  SDL_Window* handle{nullptr};
  VkExtent2D extent{1920, 1080};

  void init(VkExtent2D window_extent);
  void create_surface(VkInstance instance, VkSurfaceKHR* surface) const;
  void update_window_size();
  void cleanup() const;
};
