#pragma once

namespace gestalt::foundation {
	
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
}  // namespace gestalt