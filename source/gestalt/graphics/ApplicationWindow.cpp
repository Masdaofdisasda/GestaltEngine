#include "ApplicationWindow.h"


void ApplicationWindow::init(const VkExtent2D window_extent) {

  extent = window_extent;

  SDL_Init(SDL_INIT_VIDEO);

  constexpr auto window_flags
      = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

  handle = SDL_CreateWindow("Gestalt Engine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                             static_cast<int>(window_extent.width),
                             static_cast<int>(window_extent.height), window_flags);
  }

void ApplicationWindow::create_surface(const VkInstance instance, VkSurfaceKHR* surface) const {
  SDL_Vulkan_CreateSurface(handle, instance, surface);
  }

void ApplicationWindow::update_window_size() {
  SDL_GetWindowSize(handle, reinterpret_cast<int*>(&extent.width),
      					reinterpret_cast<int*>(&extent.height));
}

void ApplicationWindow::cleanup() const { SDL_DestroyWindow(handle);
  }
