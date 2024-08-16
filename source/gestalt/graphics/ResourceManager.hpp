#pragma once
#include <queue>

#include "VulkanTypes.hpp"
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

      IGpu* gpu_ = nullptr;
      VkCommandPool transferCommandPool = {};
      VkCommandBuffer cmd = {};
      VkFence flushFence = {};
      std::queue<ImageTask> tasks_;
      std::queue<CubemapTask> cubemap_tasks_;
      size_t max_tasks_per_batch_ = 5;

    public:
      void init(IGpu* gpu);
      void cleanup();

      void addImageTask(TextureHandle image, void* imageData, VkDeviceSize imageSize,
                        VkExtent3D imageExtent, bool mipmap);
      void execute_task(ImageTask& task);
      void execute_cubemap_task(CubemapTask& task);
      void addCubemapTask(TextureHandle image, void* imageData, VkExtent3D imageExtent);
      void add_stagging_buffer(size_t size, AllocatedBuffer& staging_buffer);
      void flush();
    };

    class ResourceManager final : public IResourceManager, public NonCopyable<ResourceManager> {

      IGpu* gpu_ = nullptr;
      Repository* repository_ = nullptr;

      void load_and_create_cubemap(const std::string& file_path, TextureHandle& cubemap);

    public:
      PoorMansResourceLoader resource_loader_;

      DescriptorWriter writer;

      void init(IGpu* gpu, Repository* repository) override;

      void cleanup() override;

      void flush_loader() override;

      std::shared_ptr<AllocatedBuffer> create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                                     VmaMemoryUsage memoryUsage,
                                                     std::string name = "") override;
      std::shared_ptr<DescriptorBuffer> create_descriptor_buffer(VkDescriptorSetLayout descriptor_layout, uint32 numBindings, VkBufferUsageFlags usage = 0, std::string name = "") override;
      void destroy_buffer(const std::shared_ptr<AllocatedBuffer> buffer) override;
      void destroy_descriptor_buffer(std::shared_ptr<DescriptorBuffer> buffer) const override;

      VkSampler create_sampler(const VkSamplerCreateInfo& sampler_create_info) const override;
      void generate_samplers() const;

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