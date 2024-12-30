#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <queue>

#include "common.hpp"
#include "VulkanTypes.hpp"
#include "Interface/IGpu.hpp"
#include "Interface/IResourceAllocator.hpp"
#include "Resources/ResourceTypes.hpp"

namespace gestalt::graphics {

  struct ImageInfo {
    int width, height;
    int channels;
    bool isEncodedData;

    explicit ImageInfo(const std::filesystem::path& path);
    ImageInfo(const unsigned char* data, size_t size, VkExtent3D extent);

    [[nodiscard]] VkExtent3D get_extent() const {
      return {static_cast<uint32>(width), static_cast<uint32>(height), 1};
    }

    [[nodiscard]] VkDeviceSize get_device_size() const {
      return static_cast<uint32>(width) * static_cast<uint32>(height) * channels;
    }

    [[nodiscard]] int get_channels() const { return channels;
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
    std::vector<unsigned char> data;
    ImageInfo image_info;

  public:
    explicit ImageData(const std::filesystem::path& path);
    explicit ImageData(std::vector<unsigned char> data, VkExtent3D extent);

    [[nodiscard]] VkExtent3D get_extent() const { return image_info.get_extent();
    }

    [[nodiscard]] VkFormat get_format() const { return image_info.get_format();
    }

    [[nodiscard]] VkDeviceSize get_image_size() const { return image_info.get_device_size(); 
    }


    [[nodiscard]] const unsigned char* get_data() const { return data.data(); }

    [[nodiscard]] int get_channels() const { return image_info.get_channels(); }


    ~ImageData() = default;
  };

  struct HdrImageData {
    std::vector<float> data;
    ImageInfo image_info;

    explicit HdrImageData(const std::filesystem::path& path);

    [[nodiscard]] VkExtent3D get_extent() const { return image_info.get_extent(); }

    [[nodiscard]] VkFormat get_format() const { return VK_FORMAT_R32G32B32A32_SFLOAT; }
    [[nodiscard]] VkDeviceSize get_image_size() const {
      return static_cast<uint32>(image_info.width) * static_cast<uint32>(image_info.height) * 4
             * sizeof(float);
    }

    [[nodiscard]] const float* get_data() const { return data.data(); }

    [[nodiscard]] int get_channels() const { return 4; }
  };

  class TaskQueue {
    IGpu* gpu_ = nullptr;
    VkCommandBuffer cmd;
    VkCommandPool command_pool_ = {};
    VkFence flushFence = {};

    struct StagingBuffer {
      VkBuffer buffer;
      VmaAllocation allocation;
    };

    std::vector<StagingBuffer> staging_buffers_;

    StagingBuffer create_staging_buffer(VkDeviceSize size) {
      StagingBuffer staging_buffer;
      VkBufferCreateInfo buffer_info = {};
      buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      buffer_info.size = size;
      buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

      VmaAllocationCreateInfo alloc_info = {};
      alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;

      if (vmaCreateBuffer(gpu_->getAllocator(), &buffer_info, &alloc_info, &staging_buffer.buffer,
                          &staging_buffer.allocation, nullptr)
          != VK_SUCCESS) {
        throw std::runtime_error("Failed to create staging buffer for image upload.");
      }

      staging_buffers_.push_back(staging_buffer);

      return staging_buffer;
    }

    void load_cubemap(const HdrImageData& image_data, VkImage image, bool mipmap);
    void load_image(const ImageData& image_data, VkImage image);

  public:
    explicit TaskQueue(IGpu* gpu) : gpu_(gpu) {
      VkCommandPoolCreateInfo pool_info = {};
      pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
      pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
      pool_info.queueFamilyIndex = gpu_->getGraphicsQueueFamily();

      VK_CHECK(vkCreateCommandPool(gpu_->getDevice(), &pool_info, nullptr, &command_pool_));

      VkCommandBufferAllocateInfo alloc_info = {};
      alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      alloc_info.commandPool = command_pool_;
      alloc_info.commandBufferCount = 1;

      VK_CHECK(vkAllocateCommandBuffers(gpu_->getDevice(), &alloc_info, &cmd));

      VkFenceCreateInfo fenceInfo = {};
      fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      VK_CHECK(vkCreateFence(gpu_->getDevice(), &fenceInfo, nullptr, &flushFence));
    }

    void add_image(const std::filesystem::path& path, VkImage image, bool is_cubemap = false, bool mipmap = false);
    void add_image(std::vector<unsigned char>& data, VkImage image, VkExtent3D extent);

    void enqueue(const std::function<void()>& task) {
      tasks_.push(task);
    }

    void processTasks() {

      if (tasks_.empty()) return;

      VK_CHECK(vkResetCommandBuffer(cmd, 0));

      VkCommandBufferBeginInfo beginInfo = {};
      beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

      while (!tasks_.empty()) {
        auto task = tasks_.front();
        task();
        tasks_.pop();
      }

      // End recording
      VK_CHECK(vkEndCommandBuffer(cmd));

      VkCommandBufferSubmitInfo commandBufferSubmitInfo = {};
      commandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
      commandBufferSubmitInfo.commandBuffer = cmd;

      VkSubmitInfo2 submitInfo2 = {};
      submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
      submitInfo2.commandBufferInfoCount = 1;
      submitInfo2.pCommandBufferInfos = &commandBufferSubmitInfo;

      VK_CHECK(vkQueueSubmit2(gpu_->getGraphicsQueue(), 1, &submitInfo2, flushFence));
      VK_CHECK(vkWaitForFences(gpu_->getDevice(), 1, &flushFence, VK_TRUE, UINT64_MAX));
      VK_CHECK(vkResetFences(gpu_->getDevice(), 1, &flushFence));
      VK_CHECK(vkResetCommandBuffer(cmd, 0));

      for (const auto& [buffer, allocation] : staging_buffers_) {
        vmaDestroyBuffer(gpu_->getAllocator(), buffer, allocation);
      }
      staging_buffers_.clear();

    }

  private:
    std::queue<std::function<void()>> tasks_;
  };

  class ResourceAllocator final : public IResourceAllocator, NonCopyable<ResourceAllocator> {

      IGpu* gpu_ = nullptr;
    TaskQueue task_queue_;

      [[nodiscard]] AllocatedImage allocate_image(std::string_view name, VkFormat format,
                                                  VkImageUsageFlags usage_flags, VkExtent3D extent,
                                                VkImageAspectFlags aspect_flags, ImageType image_type, bool mipmap = false) const;
    [[nodiscard]] AllocatedBuffer allocate_buffer(
        std::string_view name, VkDeviceSize size, VkBufferUsageFlags usage_flags,
        VkBufferCreateFlags create_flags, VkMemoryPropertyFlags memory_property_flags,
        VmaAllocationCreateFlags allocation_flags, VmaMemoryUsage memory_usage) const;

    public:
      explicit ResourceAllocator(IGpu* gpu) : gpu_(gpu), task_queue_(gpu) {}

      std::shared_ptr<ImageInstance> create_image(ImageTemplate&& image_template) override;

    std::shared_ptr<BufferInstance> create_buffer(
          BufferTemplate&& buffer_template) const override;
      void destroy_buffer(const std::shared_ptr<BufferInstance>& buffer) const override;

    void flush() { task_queue_.processTasks(); }
  };
}  // namespace gestalt