
#pragma once

#include "vk_types.hpp"
#include "GpuTypes.hpp"
#include "Window.hpp"

#include <vma/vk_mem_alloc.h>

#include <functional>


namespace gestalt::graphics {

    class Gpu final : public IGpu {

      VkInstance instance;
      VkDevice device;
      VmaAllocator allocator;

      VkQueue graphics_queue;
      uint32_t graphics_queue_family;
      VkSurfaceKHR surface;
      VkDebugUtilsMessengerEXT debug_messenger;
      VkPhysicalDevice chosen_gpu;
      VkPhysicalDeviceProperties device_properties;

    public:

      std::function<void(std::function<void(VkCommandBuffer)>)> immediate_submit;
      PFN_vkCmdDrawMeshTasksIndirectCountEXT vkCmdDrawMeshTasksIndirectCountEXT;

      void init(
          bool use_validation_layers, Window& window,
          std::function<void(std::function<void(VkCommandBuffer)>)> immediate_submit_function);
      void cleanup();

      Gpu() = default;
      ~Gpu() override;
      [[nodiscard]] VkInstance getInstance() const override;
      [[nodiscard]] VkDevice getDevice() const override;
      [[nodiscard]] VmaAllocator getAllocator() const override;
      [[nodiscard]] VkQueue getGraphicsQueue() const override;
      [[nodiscard]] uint32_t getGraphicsQueueFamily() const override;
      [[nodiscard]] VkSurfaceKHR getSurface() const override;
      [[nodiscard]] VkDebugUtilsMessengerEXT getDebugMessenger() const override;
      [[nodiscard]] VkPhysicalDevice getPhysicalDevice() const override;
      [[nodiscard]] VkPhysicalDeviceProperties getPhysicalDeviceProperties() const override;
      [[nodiscard]] std::function<void(std::function<void(VkCommandBuffer)>)> getImmediateSubmit() const override;

    };

}  // namespace gestalt