#pragma once

#include <functional>

namespace gestalt::foundation {

  class IGpu {
  public:
    virtual ~IGpu() = default;

    [[nodiscard]] virtual VkInstance getInstance() const = 0;
    [[nodiscard]] virtual VkDevice getDevice() const = 0;
    [[nodiscard]] virtual VmaAllocator getAllocator() const = 0;
    [[nodiscard]] virtual VkQueue getGraphicsQueue() const = 0;
    [[nodiscard]] virtual uint32 getGraphicsQueueFamily() const = 0;
    [[nodiscard]] virtual VkSurfaceKHR getSurface() const = 0;
    [[nodiscard]] virtual VkDebugUtilsMessengerEXT getDebugMessenger() const = 0;
    [[nodiscard]] virtual VkPhysicalDevice getPhysicalDevice() const = 0;
    [[nodiscard]] virtual VkPhysicalDeviceProperties getPhysicalDeviceProperties() const = 0;
    [[nodiscard]] virtual VkPhysicalDeviceProperties2 getPhysicalDeviceProperties2() const = 0;
    [[nodiscard]] virtual VkPhysicalDeviceDescriptorBufferPropertiesEXT getDescriptorBufferProperties()
        const = 0;
    virtual void set_debug_name(std::string_view name, VkObjectType type,
                                           uint64 handle) const
        = 0;

    virtual void immediateSubmit(std::function<void(VkCommandBuffer cmd)> function) const = 0;
  };
}  // namespace gestalt