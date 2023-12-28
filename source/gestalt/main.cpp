// Enable the WSI extensions
#if defined(__ANDROID__)
#define VK_USE_PLATFORM_ANDROID_KHR
#elif defined(__linux__)
#define VK_USE_PLATFORM_XLIB_KHR
#elif defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#endif

// Tell SDL not to mess with main()
#define SDL_MAIN_HANDLED

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <iostream>
#include <vector>

int main()
{
    // Create an SDL window that supports Vulkan rendering.
    if(SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cout << "Could not initialize SDL." << std::endl;
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("Vulkan Window", SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_VULKAN);
    if(window == NULL) {
        std::cout << "Could not create SDL window." << std::endl;
        return 1;
    }

    // Get WSI extensions from SDL (we can add more if we like - we just can't remove these)
    unsigned extension_count;
    if(!SDL_Vulkan_GetInstanceExtensions(window, &extension_count, NULL)) {
        std::cout << "Could not get the number of required instance extensions from SDL." << std::endl;
        return 1;
    }
    std::vector<const char*> extensions(extension_count);
    if(!SDL_Vulkan_GetInstanceExtensions(window, &extension_count, extensions.data())) {
        std::cout << "Could not get the names of required instance extensions from SDL." << std::endl;
        return 1;
    }

    // Use validation layers if this is a debug build
    std::vector<const char*> layers;
#if defined(_DEBUG)
    layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

    // VkApplicationInfo allows the programmer to specifiy some basic information about the
    // program, which can be useful for layers and tools to provide more debug information.
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = NULL;
    appInfo.pApplicationName = "Vulkan Program Template";
    appInfo.applicationVersion = 1;
    appInfo.pEngineName = "LunarG SDK";
    appInfo.engineVersion = 1;
    appInfo.apiVersion = VK_API_VERSION_1_0;

    // VkInstanceCreateInfo is where the programmer specifies the layers and/or extensions that
    // are needed.
    VkInstanceCreateInfo instInfo = {};
    instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instInfo.pNext = NULL;
    instInfo.flags = 0;
    instInfo.pApplicationInfo = &appInfo;
    instInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instInfo.ppEnabledExtensionNames = extensions.data();
    instInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    instInfo.ppEnabledLayerNames = layers.data();

    // Create the Vulkan instance.
    VkInstance instance;
    VkResult result = vkCreateInstance(&instInfo, NULL, &instance);
    if(result == VK_ERROR_INCOMPATIBLE_DRIVER) {
        std::cout << "Unable to find a compatible Vulkan Driver." << std::endl;
        return 1;
    } else if(result) {
        std::cout << "Could not create a Vulkan instance (for unknown reasons)." << std::endl;
        return 1;
    }

    // Create a Vulkan surface for rendering
    VkSurfaceKHR surface;
    if(!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
        std::cout << "Could not create a Vulkan surface." << std::endl;
        return 1;
    }

    // This is where most initializtion for a program should be performed

    // Poll for user input.
    bool stillRunning = true;
    while(stillRunning) {

        SDL_Event event;
        while(SDL_PollEvent(&event)) {

            switch(event.type) {

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
    vkDestroySurfaceKHR(instance, surface, NULL);
    SDL_DestroyWindow(window);
    SDL_Quit();
    vkDestroyInstance(instance, NULL);

    return 0;
}
