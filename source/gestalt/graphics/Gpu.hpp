
#pragma once

#include "vk_types.hpp"
#include "GpuTypes.hpp"
#include "Window.hpp"

#include <vma/vk_mem_alloc.h>

#include <functional>


namespace gestalt::graphics {

    class Gpu final : public IGpu, public NonCopyable<Gpu> {

      VkInstance instance;
      VkDevice device;
      VmaAllocator allocator;

      VkQueue graphics_queue;
      uint32_t graphics_queue_family;
      VkSurfaceKHR surface;
      VkDebugUtilsMessengerEXT debug_messenger;
      VkPhysicalDevice chosen_gpu;
      VkPhysicalDeviceProperties device_properties{};
      VkPhysicalDeviceProperties2 device_properties2{
          .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
      VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptorBufferProperties{
          .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT};

      VkFence immediate_submit_fence_ = VK_NULL_HANDLE;
      VkCommandPool immediate_submit_command_pool_ = VK_NULL_HANDLE;
      VkCommandBuffer immediate_submit_command_buffer_ = VK_NULL_HANDLE;

    public:

      void init(Window* window);
      void cleanup() const;

      Gpu() = default;
      ~Gpu() override;
      [[nodiscard]] VkInstance getInstance() const override;
      [[nodiscard]] VkDevice getDevice() const override;
      [[nodiscard]] VmaAllocator getAllocator() const override;
      [[nodiscard]] VkQueue getGraphicsQueue() const override;
      [[nodiscard]] uint32 getGraphicsQueueFamily() const override;
      [[nodiscard]] VkSurfaceKHR getSurface() const override;
      [[nodiscard]] VkDebugUtilsMessengerEXT getDebugMessenger() const override;
      [[nodiscard]] VkPhysicalDevice getPhysicalDevice() const override;
      [[nodiscard]] VkPhysicalDeviceProperties getPhysicalDeviceProperties() const override;
      [[nodiscard]] VkPhysicalDeviceProperties2 getPhysicalDeviceProperties2() const override;
      [[nodiscard]] VkPhysicalDeviceDescriptorBufferPropertiesEXT getDescriptorBufferProperties()
          const override;
      void immediateSubmit(std::function<void(VkCommandBuffer cmd)> function) const override;
    };

}  // namespace gestalt