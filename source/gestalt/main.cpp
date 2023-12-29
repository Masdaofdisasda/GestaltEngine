#if defined(_WIN32)
#  define VK_USE_PLATFORM_WIN32_KHR
#endif

#define SDL_MAIN_HANDLED
#define VOLK_IMPLEMENTATION

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <Volk/volk.h>

#include <iostream>
#include <vector>

int main(int argc, char** argv) {
  // Initialize Volk
  if (volkInitialize() != VK_SUCCESS) {
    std::cerr << "Failed to initialize Volk" << std::endl;
    return 1;
  }

  // Initialize SDL with video support.
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cerr << "Could not initialize SDL: " << SDL_GetError() << std::endl;
    return 1;
  }

  // Create an SDL window that supports Vulkan rendering.
  SDL_Window* window = SDL_CreateWindow("Vulkan Window", SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_VULKAN);
  if (window == nullptr) {
    std::cerr << "Could not create SDL window: " << SDL_GetError() << std::endl;
    SDL_Quit();
    return 1;
  }

  // Get WSI extensions from SDL
  unsigned extension_count = 0;
  if (!SDL_Vulkan_GetInstanceExtensions(window, &extension_count, nullptr)) {
    std::cerr << "Could not get the number of required instance extensions from SDL." << std::endl;
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  std::vector<const char*> extensions(extension_count);
  if (!SDL_Vulkan_GetInstanceExtensions(window, &extension_count, extensions.data())) {
    std::cerr << "Could not get the names of required instance extensions from SDL." << std::endl;
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  // Use validation layers in debug mode
  std::vector<const char*> layers;
#if defined(_DEBUG)
  layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

  // VkApplicationInfo setup
  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "Vulkan Program Template";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "No Engine";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_0;

  // VkInstanceCreateInfo setup
  VkInstanceCreateInfo instInfo = {};
  instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instInfo.pApplicationInfo = &appInfo;
  instInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  instInfo.ppEnabledExtensionNames = extensions.data();
  instInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
  instInfo.ppEnabledLayerNames = layers.data();

  // Create the Vulkan instance
  VkInstance instance;
  VkResult result = vkCreateInstance(&instInfo, nullptr, &instance);
  if (result != VK_SUCCESS) {
    std::cerr << "Could not create a Vulkan instance: " << result << std::endl;
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  volkLoadInstance(instance);

  // Create a Vulkan surface for rendering
  VkSurfaceKHR surface;
  if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
    std::cerr << "Could not create a Vulkan surface." << std::endl;
    vkDestroyInstance(instance, nullptr);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  // This is where most initializtion for a program should be performed

  // Poll for user input.
  bool stillRunning = true;
  while (stillRunning) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_QUIT:
          stillRunning = false;
          break;

        default:
          // Do nothing.
          break;
      }
    }

    SDL_Delay(10);
  }

  // Clean up.
  vkDestroySurfaceKHR(instance, surface, nullptr);
  vkDestroyInstance(instance, nullptr);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
