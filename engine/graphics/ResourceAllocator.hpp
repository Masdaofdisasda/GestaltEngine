#pragma once

#include <memory>

#include "TaskQueue.hpp"
#include "common.hpp"
#include "VulkanCore.hpp"
#include "Interface/IGpu.hpp"
#include "Interface/IResourceAllocator.hpp"
#include "Resources/ResourceTypes.hpp"

namespace gestalt::graphics {

  class ResourceAllocator final : public IResourceAllocator {

      IGpu& gpu_;
    TaskQueue task_queue_;

      [[nodiscard]] AllocatedImage allocate_image(const std::string& name, VkFormat format,
                                                  VkImageUsageFlags usage_flags, VkExtent3D extent,
                                                VkImageAspectFlags aspect_flags, ImageType image_type, bool mipmap = false) const;
    [[nodiscard]] AllocatedBuffer allocate_buffer(
        std::string_view name, VkDeviceSize size, VkBufferUsageFlags usage_flags,
        VkBufferCreateFlags create_flags, VkMemoryPropertyFlags memory_property_flags,
        VmaAllocationCreateFlags allocation_flags, VmaMemoryUsage memory_usage) const;

    public:
      explicit ResourceAllocator(IGpu& gpu) : gpu_(gpu), task_queue_(gpu) {}
      ~ResourceAllocator() override = default;

      ResourceAllocator(const ResourceAllocator&) = delete;
      ResourceAllocator& operator=(const ResourceAllocator&) = delete;

      ResourceAllocator(ResourceAllocator&&) = delete;
      ResourceAllocator& operator=(ResourceAllocator&&) = delete;

      std::shared_ptr<ImageInstance> create_image(ImageTemplate&& image_template) override;

    std::shared_ptr<BufferInstance> create_buffer(
          BufferTemplate&& buffer_template) const override;
      void destroy_buffer(const std::shared_ptr<BufferInstance>& buffer) const override;

    void flush() { task_queue_.process_tasks(); }
  };
}  // namespace gestalt