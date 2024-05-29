#pragma once
#include <queue>

#include "vk_types.hpp"
#include "vk_descriptors.hpp"
#include "Repository.hpp"


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

      std::shared_ptr<IGpu> gpu_;
      VkCommandPool transferCommandPool = {};
      VkCommandBuffer cmd = {};
      VkFence flushFence = {};
      std::queue<ImageTask> tasks_;
      std::queue<CubemapTask> cubemap_tasks_;
      size_t max_tasks_per_batch_ = 5;

    public:
      void init(const std::shared_ptr<IGpu>& gpu);
      void cleanup();

      void addImageTask(TextureHandle image, void* imageData, VkDeviceSize imageSize,
                        VkExtent3D imageExtent, bool mipmap);
      void execute_task(ImageTask& task);
      void execute_cubemap_task(CubemapTask& task);
      void addCubemapTask(TextureHandle image, void* imageData, VkExtent3D imageExtent);
      void add_stagging_buffer(size_t size, AllocatedBuffer& staging_buffer);
      void flush();
    };

    class ResourceManager final : public IResourceManager {

      std::shared_ptr<IGpu> gpu_;
      std::shared_ptr<Repository> repository_;

      void load_and_create_cubemap(const std::string& file_path, TextureHandle& cubemap);

    public:
      DescriptorAllocatorGrowable* descriptor_pool;

      PoorMansResourceLoader resource_loader_;

      std::shared_ptr<IDescriptorAllocatorGrowable> descriptorPool = std::make_shared<DescriptorAllocatorGrowable>();
      DescriptorWriter writer;

      void init(const std::shared_ptr<IGpu>& gpu, const std::shared_ptr<Repository>& repository) override;

      void cleanup() override;

      void flush_loader() override;

      std::shared_ptr<IDescriptorAllocatorGrowable>& get_descriptor_pool() override;

      AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                    VmaMemoryUsage memoryUsage) override;
      void upload_mesh() override;
      void destroy_buffer(const AllocatedBuffer& buffer) override;

      VkSampler create_sampler(const VkSamplerCreateInfo& sampler_create_info) const override;

      TextureHandle create_image(void* data, VkExtent3D size, VkFormat format,
                                 VkImageUsageFlags usage, bool mipmapped = false) override;
      TextureHandle create_cubemap(void* imageData, VkExtent3D size, VkFormat format,
                                   VkImageUsageFlags usage, bool mipmapped = false) override;
      std::optional<TextureHandle> load_image(const std::string& filepath) override;
      void load_and_process_cubemap(const std::string& file_path) override;
      TextureHandle create_cubemap_from_HDR(std::vector<float>& image_data, int h, int w) override;
      void create_frame_buffer(const std::shared_ptr<TextureHandle>& image,
                               VkFormat format = VK_FORMAT_UNDEFINED) const override;
      TextureHandle create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                                 bool mipmapped = false, bool cubemap = false) override;
      void destroy_image(const TextureHandle& img) override;
    };
}  // namespace gestalt