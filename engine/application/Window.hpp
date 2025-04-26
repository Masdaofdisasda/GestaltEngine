#pragma once

#include "common.hpp"

struct SDL_Window;
typedef struct VkInstance_T* VkInstance;
typedef struct VkSurfaceKHR_T* VkSurfaceKHR;

namespace gestalt::application {

    class Window {
      SDL_Window* handle_;
      uint32 width_, height_;
    public:
      Window();
      ~Window();

      Window(const Window&) = delete;
      Window& operator=(const Window&) = delete;

      Window(Window&&) = delete;
      Window& operator=(Window&&) = delete;

      void create_surface(VkInstance instance, VkSurfaceKHR* surface) const;
      void update_window_size();
      void capture_mouse() const;
      void release_mouse() const;
      [[nodiscard]] uint32 get_width() const;
      [[nodiscard]] uint32 get_height() const;
      [[nodiscard]] SDL_Window* get_handle() const;
    };
}  // namespace gestalt