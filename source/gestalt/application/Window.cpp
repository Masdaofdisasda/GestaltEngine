#include "Window.hpp"

#include <SDL.h>
#include <SDL_vulkan.h>

#include "EngineConfiguration.hpp"


namespace gestalt::application {

    void Window::init() {

      SDL_Init(SDL_INIT_VIDEO);

      if (useFullscreen()) {
        constexpr auto window_flags = static_cast<SDL_WindowFlags>(
            SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
            | SDL_WINDOW_FULLSCREEN_DESKTOP);

        handle = SDL_CreateWindow(getApplicationName().c_str(), SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED, 0,
                                  0, window_flags);
        update_window_size();
        return;
      }

      extent = {getWindowedWidth(), getWindowedHeight()};


      constexpr auto window_flags = static_cast<SDL_WindowFlags>(
          SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);

      handle = SDL_CreateWindow(getApplicationName().c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                static_cast<int>(extent.width), static_cast<int>(extent.height),
                                window_flags);
    }

    void Window::create_surface(const VkInstance instance, VkSurfaceKHR* surface) const {
      SDL_Vulkan_CreateSurface(handle, instance, surface);
    }

    void Window::update_window_size() {
      SDL_GetWindowSize(handle, reinterpret_cast<int*>(&extent.width),
                        reinterpret_cast<int*>(&extent.height));
    }

    void Window::capture_mouse() const {
      SDL_SetRelativeMouseMode(SDL_TRUE);
    }

    void Window::release_mouse() const {
      SDL_SetRelativeMouseMode(SDL_FALSE);
    }


    void Window::cleanup() const { SDL_DestroyWindow(handle); }

}  // namespace gestalt