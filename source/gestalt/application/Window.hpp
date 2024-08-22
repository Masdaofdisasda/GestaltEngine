#pragma once

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "common.hpp"
#include "VulkanTypes.hpp"

namespace gestalt::application {

    class Window : public NonCopyable<Window> {
    public:
      SDL_Window* handle{nullptr};
      VkExtent2D extent{1920, 1080};

      void init();
      void create_surface(VkInstance instance, VkSurfaceKHR* surface) const;
      void update_window_size();
      void cleanup() const;
    };
}  // namespace gestalt