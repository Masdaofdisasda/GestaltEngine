#pragma once
#include <memory>
#include <optional>
#include <vector>

#include "BasicResourceLoader.hpp"
#include "common.hpp"
#include "VulkanTypes.hpp"
#include "Interface/IResourceManager.hpp"
#include "Utils/vk_descriptors.hpp"

namespace gestalt::foundation {
  struct AllocatedBuffer;
  struct DescriptorBuffer;
  class Repository;
  class IGpu;
}

namespace gestalt::graphics {

    class ResourceManager final : public foundation::IResourceManager, public NonCopyable<ResourceManager> {

      IGpu* gpu_ = nullptr;
      Repository* repository_ = nullptr;

      void load_and_create_cubemap(const std::string& file_path, TextureHandle& cubemap);

    public:
      std::unique_ptr<DescriptorWriter> descriptor_writer_;

      std::unique_ptr<BasicResourceLoader> resource_loader_;

      void init(IGpu* gpu, Repository* repository) override;

      void cleanup() override;

      void flush_loader() override;
      void SetDebugName(const std::string& name, VkObjectType type, VkDevice device,
                               uint64_t handle) const override;

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
      void create_3D_image(const std::shared_ptr<TextureHandle>& image, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, const std::string& name) const;
      TextureHandle create_cubemap(void* imageData, VkExtent3D size, VkFormat format,
                                   VkImageUsageFlags usage, bool mipmapped = false) override;
      std::optional<TextureHandle> load_image(const std::string& filepath) override;
      void load_and_process_cubemap(const std::string& file_path) override;
      TextureHandle create_cubemap_from_HDR(std::vector<float>& image_data, int h, int w) override;
      void create_frame_buffer(const std::shared_ptr<TextureHandle>& image,
                               const std::string& name,
                               VkFormat format = VK_FORMAT_UNDEFINED) const override;
      TextureHandle create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                                 bool mipmapped = false, bool cubemap = false) override;
      void destroy_image(const TextureHandle& img) override;
    };
}  // namespace gestalt