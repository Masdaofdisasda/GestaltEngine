#pragma once

namespace gestalt::foundation {

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
}  // namespace gestalt