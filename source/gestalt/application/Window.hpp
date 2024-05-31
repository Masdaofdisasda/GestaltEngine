#pragma once

#include <SDL.h>
#include <SDL_vulkan.h>
#include "vk_types.hpp"

namespace gestalt::application {

    class Window {
    public:
      SDL_Window* handle{nullptr};
      VkExtent2D extent{1920, 1080};

      void init();
      void create_surface(VkInstance instance, VkSurfaceKHR* surface) const;
      void update_window_size();
      void cleanup() const;
    };
}  // namespace gestalt