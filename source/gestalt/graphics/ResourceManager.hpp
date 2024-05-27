#pragma once
#include <queue>
#include <fastgltf/types.hpp>

#include "Repository.hpp"
#include "Gpu.hpp"
#include "vk_types.hpp"
#include "vk_descriptors.hpp"


namespace gestalt::graphics {

    class PoorMansResourceLoader {
      struct ImageTask {
        AllocatedBuffer stagingBuffer;
        std::shared_ptr<TextureHandle> image;
        unsigned char* dataCopy;
        VkDeviceSize imageSize;
        VkExtent3D imageExtent;
        bool mipmap;
      };

      struct CubemapTask {
        AllocatedBuffer stagingBuffer;
        std::shared_ptr<TextureHandle> image;
        unsigned char* dataCopy;
        VkDeviceSize totalCubemapSizeBytes;
        VkDeviceSize faceSizeBytes;
        VkExtent3D imageExtent;
      };

      Gpu gpu_ = {};
      VkCommandPool transferCommandPool = {};
      VkCommandBuffer cmd = {};
      VkFence flushFence = {};
      std::queue<ImageTask> tasks_;
      std::queue<CubemapTask> cubemap_tasks_;
      const size_t max_tasks_per_batch_ = 5;

    public:
      void init(const Gpu& gpu);
      void cleanup();

      void addImageTask(TextureHandle image, void* imageData, VkDeviceSize imageSize,
                        VkExtent3D imageExtent, bool mipmap);
      void execute_task(ImageTask& task);
      void execute_cubemap_task(CubemapTask& task);
      void addCubemapTask(TextureHandle image, void* imageData, VkExtent3D imageExtent);
      void add_stagging_buffer(size_t size, AllocatedBuffer& staging_buffer);
      void flush();
    };

    class ResourceManager {
      Gpu gpu_ = {};
      std::shared_ptr<Repository> repository_;

      void load_and_create_cubemap(const std::string& file_path, TextureHandle& cubemap);

    public:
      DescriptorAllocatorGrowable* descriptor_pool;

      PoorMansResourceLoader resource_loader_;

      DescriptorAllocatorGrowable descriptorPool;  // TODO hide this
      descriptor_writer writer;

      void init(const Gpu& gpu, const std::shared_ptr<Repository>& repository);

      void cleanup();

      AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                    VmaMemoryUsage memoryUsage);
      void upload_mesh();
      void destroy_buffer(const AllocatedBuffer& buffer);

      VkSampler create_sampler(const VkSamplerCreateInfo& sampler_create_info) const;

      TextureHandle create_image(void* data, VkExtent3D size, VkFormat format,
                                 VkImageUsageFlags usage, bool mipmapped = false);
      TextureHandle create_cubemap(void* imageData, VkExtent3D size, VkFormat format,
                                   VkImageUsageFlags usage, bool mipmapped = false);
      std::optional<TextureHandle> load_image(const std::string& filepath);
      void load_and_process_cubemap(const std::string& file_path);
      TextureHandle create_cubemap_from_HDR(std::vector<float>& image_data, int h, int w);
      void create_frame_buffer(const std::shared_ptr<TextureHandle>& image,
                               VkFormat format = VK_FORMAT_UNDEFINED) const;
      TextureHandle create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                                 bool mipmapped = false, bool cubemap = false);
      void destroy_image(const TextureHandle& img);
    };
}  // namespace gestalt