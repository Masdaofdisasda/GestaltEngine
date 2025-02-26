#pragma once

#include "VulkanTypes.hpp"
#include "Interface/IGpu.hpp"
#include "Window.hpp"

#include <vk_mem_alloc.h>

#include <functional>

#include "common.hpp"


namespace gestalt::graphics {

    class Gpu final : public IGpu{

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
      VkPhysicalDeviceAccelerationStructurePropertiesKHR acceleration_structure_properties{
          .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};

      VkFence immediate_submit_fence_ = VK_NULL_HANDLE;
      VkCommandPool immediate_submit_command_pool_ = VK_NULL_HANDLE;
      VkCommandBuffer immediate_submit_command_buffer_ = VK_NULL_HANDLE;

    public:

      void init(Window* window);
      void cleanup() const;

      Gpu() = default;
      ~Gpu() override;

      Gpu(const Gpu&) = delete;
      Gpu& operator=(const Gpu&) = delete;

      Gpu(Gpu&&) = delete;
      Gpu& operator=(Gpu&&) = delete;

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

      [[nodiscard]] VkPhysicalDeviceAccelerationStructurePropertiesKHR
      getAccelerationStructureProperties() const override;
      void set_debug_name(std::string_view name, VkObjectType type,
                                             uint64 handle) const override;
      void immediateSubmit(std::function<void(VkCommandBuffer cmd)> function) const override;
    };

}  // namespace gestalt