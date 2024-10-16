﻿#include "Window.hpp"
#include <SDL_vulkan.h>

#include "EngineConfig.hpp"


namespace gestalt::application {

    void Window::init() {
    extent = {getResolutionWidth(), getResolutionHeight()};

      SDL_Init(SDL_INIT_VIDEO);

      constexpr auto window_flags = static_cast<SDL_WindowFlags>(
          SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);

      handle = SDL_CreateWindow("Gestalt Engine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
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