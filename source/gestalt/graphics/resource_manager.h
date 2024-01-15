#pragma once
#include "vk_types.h"

class resource_manager {
  VkDevice device_;
  VmaAllocator allocator_;
  std::function<void(std::function<void(VkCommandBuffer)>)> immediate_submit_function_;

public:
  void init(const VkDevice device, const VmaAllocator allocator,
            std::function<void(std::function<void(VkCommandBuffer)>)> immediate_submit_function) {
    device_ = device;
    allocator_ = allocator;
    immediate_submit_function_ = immediate_submit_function;
  }

  AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                VmaMemoryUsage memoryUsage);
  void destroy_buffer(const AllocatedBuffer& buffer);

  AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                              bool mipmapped = false);
  AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                              bool mipmapped = false);
  void destroy_image(const AllocatedImage& img);
};
