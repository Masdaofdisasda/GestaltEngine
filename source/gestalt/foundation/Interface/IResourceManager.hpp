#pragma once
#include <Resources/AllocatedBufferOld.hpp>
#include <Repository.hpp>

namespace gestalt::foundation {
  class IGpu;

  class IResourceManager {
  public:
    virtual ~IResourceManager() = default;

    virtual void init(IGpu* gpu, Repository* repository)
        = 0;
    virtual void cleanup() = 0;

    virtual void flush_loader() = 0;

    virtual std::shared_ptr<AllocatedBufferOld> create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                                           VmaMemoryUsage memoryUsage,
                                                           std::string name = "")
        = 0;
    virtual std::shared_ptr<DescriptorBuffer> create_descriptor_buffer(
        VkDescriptorSetLayout descriptor_layout, uint32 numBindings, VkBufferUsageFlags usage = 0 ,
        std::string name = "")
    = 0;
    virtual void destroy_buffer(const std::shared_ptr<AllocatedBufferOld> buffer) = 0;

    virtual void destroy_descriptor_buffer(std::shared_ptr<DescriptorBuffer> buffer) const = 0;

    virtual VkSampler create_sampler(const VkSamplerCreateInfo& sampler_create_info) const = 0;

    virtual TextureHandleOld create_image(void* data, VkExtent3D size, VkFormat format,
                                       VkImageUsageFlags usage, bool mipmapped = false)
        = 0;
    virtual TextureHandleOld create_cubemap(void* imageData, VkExtent3D size, VkFormat format,
                                         VkImageUsageFlags usage, bool mipmapped = false)
        = 0;
    virtual std::optional<TextureHandleOld> load_image(const std::string& filepath) = 0;
    virtual void load_and_process_cubemap(const std::string& file_path) = 0;
    virtual TextureHandleOld create_cubemap_from_HDR(std::vector<float>& image_data, int h, int w) = 0;
    virtual void create_frame_buffer(const std::shared_ptr<TextureHandleOld>& image,
                                     const std::string& name,
                                     VkFormat format = VK_FORMAT_UNDEFINED) const
        = 0;
    virtual TextureHandleOld create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                                       bool mipmapped = false, bool cubemap = false)
        = 0;
    virtual void destroy_image(const TextureHandleOld& img) = 0;

    virtual void SetDebugName(const std::string& name, VkObjectType type, VkDevice device,
                               uint64_t handle) const
        = 0;
  };
}  // namespace gestalt