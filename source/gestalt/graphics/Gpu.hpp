
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
      VkPhysicalDeviceProperties device_properties;

      VkFence immediate_submit_fence_ = VK_NULL_HANDLE;
      VkCommandPool immediate_submit_command_pool_ = VK_NULL_HANDLE;
      VkCommandBuffer immediate_submit_command_buffer_ = VK_NULL_HANDLE;

      PFN_vkCmdDrawMeshTasksIndirectCountEXT vkCmdDrawMeshTasksIndirectCountEXT;
      PFN_vkCmdDrawMeshTasksIndirectEXT vkCmdDrawMeshTasksIndirectEXT;

    public:

      void init(Window* window);
      void cleanup() const;

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
      void immediateSubmit(std::function<void(VkCommandBuffer cmd)> function) const override;
      void drawMeshTasksIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer,
                                      VkDeviceSize offset, VkBuffer countBuffer,
                                      VkDeviceSize countBufferOffset, uint32_t maxDrawCount,
                                      uint32_t stride) override;
      void drawMeshTasksIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer,
                                          VkDeviceSize offset, uint32_t drawCount, uint32_t stride) const override;
    };

}  // namespace gestalt