#pragma once

#include <memory>
#include <mutex>
#include <queue>

#include "common.hpp"
#include "VulkanTypes.hpp"
#include "Interface/IGpu.hpp"
#include "Render Engine/FrameGraphTypes.hpp"

namespace gestalt::graphics::fg {
  struct BufferResource;
  struct BufferResourceTemplate;
  struct ImageResource;
  struct ImageResourceTemplate;
}

namespace gestalt::graphics {

  
  struct ImageInfo {
    int width, height;
    int channels;
    explicit ImageInfo(const std::filesystem::path& path);

    [[nodiscard]] VkExtent3D get_extent() const {
      return {static_cast<uint32>(width), static_cast<uint32>(height), 1};
    }

    [[nodiscard]] VkFormat get_format() const {
      switch (channels) {
        case 1:
          return VK_FORMAT_R8_UNORM;
        case 2:
          return VK_FORMAT_R8G8_UNORM;
        case 3:
          // return VK_FORMAT_R8G8B8_UNORM; // not supported
        case 4:
          return VK_FORMAT_R8G8B8A8_UNORM;
        default:
          throw std::runtime_error("Unsupported number of channels in image data.");
      }
    }
  };

  struct ImageData {
  private:
    unsigned char* data;
    ImageInfo image_info;

  public:
    explicit ImageData(const std::filesystem::path& path);

    [[nodiscard]] VkExtent3D get_extent() const { return image_info.get_extent();
    }

    [[nodiscard]] VkFormat get_format() const { return image_info.get_format();
    }

    [[nodiscard]] const unsigned char* get_data() const { return data; }

    ~ImageData();
  };

  class TaskQueue {
  public:
    void enqueue(std::function<void(VkCommandBuffer cmd)> task) {
      std::lock_guard<std::mutex> lock(mutex_);
      tasks_.push(task);
      condition_.notify_one();
    }

    void processTasks(VkCommandBuffer cmd) {
      std::function<void(VkCommandBuffer cmd)> task;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (tasks_.empty()) return;
        task = std::move(tasks_.front());
        tasks_.pop();
      }
      // Execute the task
      task(cmd);
    }

  private:
    std::queue<std::function<void(VkCommandBuffer cmd)>> tasks_;
    std::mutex mutex_;
    std::condition_variable condition_;
  };

  class ResourceFactory final : public NonCopyable<ResourceFactory> {

      IGpu* gpu_ = nullptr;
    TaskQueue task_queue_{};

    fg::AllocatedImage allocate_image(std::string_view name, VkFormat format,
                                      VkImageUsageFlags usage_flags, VkExtent3D extent,
                                      VkImageAspectFlags aspect_flags) const;
      fg::AllocatedBuffer allocate_buffer(std::string_view name, VkDeviceSize size,
                                          VkBufferUsageFlags usage_flags,
                                          VmaMemoryUsage memory_usage) const;

    public:
      explicit ResourceFactory(IGpu* gpu) : gpu_(gpu) {}

      void set_debug_name(std::string_view name, VkObjectType type,
                          uint64 handle) const;
      std::shared_ptr<fg::ImageResource> create_image(fg::ImageResourceTemplate&& image_template);

    std::shared_ptr<fg::BufferResource> create_buffer(
          fg::BufferResourceTemplate&& buffer_template) const;
    };
}  // namespace gestalt