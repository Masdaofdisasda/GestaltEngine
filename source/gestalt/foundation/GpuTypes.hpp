#pragma once

#include "VulkanTypes.hpp"
#include <vma/vk_mem_alloc.h>
#include "GpuResources.hpp"

#include <functional>
#include <optional>
#include <span>

namespace gestalt::foundation {
  class Repository;
  class IDescriptorAllocatorGrowable;

  class IGpu {
  public:
    virtual ~IGpu() = default;

    [[nodiscard]] virtual VkInstance getInstance() const = 0;
    [[nodiscard]] virtual VkDevice getDevice() const = 0;
    [[nodiscard]] virtual VmaAllocator getAllocator() const = 0;
    [[nodiscard]] virtual VkQueue getGraphicsQueue() const = 0;
    [[nodiscard]] virtual uint32 getGraphicsQueueFamily() const = 0;
    [[nodiscard]] virtual VkSurfaceKHR getSurface() const = 0;
    [[nodiscard]] virtual VkDebugUtilsMessengerEXT getDebugMessenger() const = 0;
    [[nodiscard]] virtual VkPhysicalDevice getPhysicalDevice() const = 0;
    [[nodiscard]] virtual VkPhysicalDeviceProperties getPhysicalDeviceProperties() const = 0;
    [[nodiscard]] virtual VkPhysicalDeviceProperties2 getPhysicalDeviceProperties2() const = 0;
    [[nodiscard]] virtual VkPhysicalDeviceDescriptorBufferPropertiesEXT getDescriptorBufferProperties()
        const = 0;

    virtual void immediateSubmit(std::function<void(VkCommandBuffer cmd)> function) const = 0;
  };

  class IResourceManager {
  public:
    virtual ~IResourceManager() = default;

    virtual void init(IGpu* gpu, Repository* repository)
        = 0;
    virtual void cleanup() = 0;

    virtual void flush_loader() = 0;

    virtual std::shared_ptr<AllocatedBuffer> create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                                           VmaMemoryUsage memoryUsage,
                                                           std::string name = "")
        = 0;
    virtual std::shared_ptr<DescriptorBuffer> create_descriptor_buffer(
        VkDescriptorSetLayout descriptor_layout, uint32 numBindings, VkBufferUsageFlags usage = 0 ,
        std::string name = "")
    = 0;
    virtual void destroy_buffer(const std::shared_ptr<AllocatedBuffer> buffer) = 0;

    virtual void destroy_descriptor_buffer(std::shared_ptr<DescriptorBuffer> buffer) const = 0;

    virtual VkSampler create_sampler(const VkSamplerCreateInfo& sampler_create_info) const = 0;

    virtual TextureHandle create_image(void* data, VkExtent3D size, VkFormat format,
                                       VkImageUsageFlags usage, bool mipmapped = false)
        = 0;
    virtual TextureHandle create_cubemap(void* imageData, VkExtent3D size, VkFormat format,
                                         VkImageUsageFlags usage, bool mipmapped = false)
        = 0;
    virtual std::optional<TextureHandle> load_image(const std::string& filepath) = 0;
    virtual void load_and_process_cubemap(const std::string& file_path) = 0;
    virtual TextureHandle create_cubemap_from_HDR(std::vector<float>& image_data, int h, int w) = 0;
    virtual void create_frame_buffer(const std::shared_ptr<TextureHandle>& image,
                                     VkFormat format = VK_FORMAT_UNDEFINED) const
        = 0;
    virtual TextureHandle create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                                       bool mipmapped = false, bool cubemap = false)
        = 0;
    virtual void destroy_image(const TextureHandle& img) = 0;
  };

  class IDescriptorLayoutBuilder {
  public:
    virtual ~IDescriptorLayoutBuilder() = default;

    virtual IDescriptorLayoutBuilder& add_binding(uint32_t binding, VkDescriptorType type,
                                                  VkShaderStageFlags shader_stages,
                                                  bool bindless = false,
                                                  uint32_t descriptor_count = 1)
        = 0;
    virtual void clear() = 0;
    virtual VkDescriptorSetLayout build(VkDevice device, VkDescriptorSetLayoutCreateFlags flags = 0)
        = 0;
  };

  class IDescriptorAllocatorGrowable {
  public:
    struct PoolSizeRatio {
      VkDescriptorType type;
      float32 ratio;
    };

    virtual ~IDescriptorAllocatorGrowable() = default;

    virtual void init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios)
        = 0;
    virtual void clear_pools(VkDevice device) = 0;
    virtual void destroy_pools(VkDevice device) = 0;

    virtual VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout,
                                     const std::vector<uint32_t>& variableDescriptorCounts = {})
        = 0;
  };

  class IDescriptorWriter {
  public:
    virtual ~IDescriptorWriter() = default;

    virtual void write_image(int binding, VkImageView image, VkSampler sampler,
                             VkImageLayout layout, VkDescriptorType type)
        = 0;
    virtual void write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset,
                              VkDescriptorType type)
        = 0;
    virtual void write_buffer_array(int binding,
                                    const std::vector<VkDescriptorBufferInfo>& bufferInfos,
                                    VkDescriptorType type, uint32_t arrayElementStart)
        = 0;
    virtual void write_image_array(int binding,
                                   const std::array<VkDescriptorImageInfo, 5>& imageInfos,
                                   uint32_t arrayElementStart = 0)
        = 0;

    virtual void clear() = 0;
    virtual void update_set(VkDevice device, VkDescriptorSet set) = 0;
  };
}  // namespace gestalt::foundation