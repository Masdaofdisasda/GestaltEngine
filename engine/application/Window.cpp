#include "Window.hpp"

#include "VulkanCore.hpp"
#include <SDL.h>
#include <SDL_vulkan.h>

#include "EngineConfiguration.hpp"

namespace gestalt::application {

  Window::Window() {
    SDL_Init(SDL_INIT_VIDEO);

    if (useFullscreen()) {
      constexpr auto window_flags = static_cast<SDL_WindowFlags>(
          SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
          | SDL_WINDOW_FULLSCREEN_DESKTOP);

      handle_ = SDL_CreateWindow(getApplicationName().c_str(), SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED, 0, 0, window_flags);
      update_window_size();

      // TODO improve this:
      EngineConfiguration::get_instance().load_from_file();
      auto config = EngineConfiguration::get_instance().get_config();
      config.windowedHeight = height_;
      config.windowedWidth = width_;
      EngineConfiguration::get_instance().set_config(config);
      return;
    }

    width_ = getWindowedWidth();
    height_ = getWindowedHeight();

    constexpr auto window_flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN
                                                               | SDL_WINDOW_ALLOW_HIGHDPI);

    handle_ = SDL_CreateWindow(getApplicationName().c_str(), SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED, static_cast<int>(width_),
                              static_cast<int>(height_), window_flags);
  }

  Window::~Window() {
    SDL_DestroyWindow(handle_);
  }

  void Window::create_surface(const VkInstance instance, VkSurfaceKHR* surface) const {
    SDL_Vulkan_CreateSurface(handle_, instance, surface);
  }

  void Window::update_window_size() {
    SDL_GetWindowSize(handle_, reinterpret_cast<int*>(&width_), reinterpret_cast<int*>(&height_));
  }

  void Window::capture_mouse() const { SDL_SetRelativeMouseMode(SDL_TRUE); }

  void Window::release_mouse() const { SDL_SetRelativeMouseMode(SDL_FALSE); }

  uint32 Window::get_width() const { return width_; }

  uint32 Window::get_height() const { return height_; }

  SDL_Window* Window::get_handle() const { return handle_; }

}  // namespace gestalt::application