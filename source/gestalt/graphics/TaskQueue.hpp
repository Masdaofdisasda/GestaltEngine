#pragma once

#include <filesystem>
#include <queue>

#include "Interface/IGpu.hpp"
#include "common.hpp"
#include "VulkanTypes.hpp"

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

    StagingBuffer create_staging_buffer(VkDeviceSize size);

    void load_cubemap(const HdrImageData& image_data, VkImage image, bool mipmap);
    void load_image(const ImageData& image_data, VkImage image, bool mipmap);

  public:
    explicit TaskQueue(IGpu* gpu);

    void add_image(const std::filesystem::path& path, VkImage image, bool is_cubemap = false, bool mipmap = false);
    void add_image(std::vector<unsigned char>& data, VkImage image, VkExtent3D extent, bool mipmap = false);

    void enqueue(const std::function<void()>& task) {
      tasks_.push(task);
    }

    void process_tasks();

  private:
    std::queue<std::function<void()>> tasks_;
  };

}  // namespace gestalt