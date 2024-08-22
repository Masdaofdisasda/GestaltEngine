#pragma once
#include <vector>

#include "DescriptorBinding.hpp"
#include "DescriptorUpdate.hpp"

#include <vma/vk_mem_alloc.h>

namespace gestalt::foundation {

  
  /**
   * \brief Descriptor Buffer replaces Descriptor Set in order to be able to record updates into the Command Buffer
   * Each Descriptor Buffer should map to one Set in GLSL
   */
  struct DescriptorBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
    VkDeviceAddress address;
    VkDeviceSize size;
    // these flags are required for descriptor buffers:
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
                               | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    // Each offset corresponds to one layout binding in GLSL
    // Bindings need to start at 0 and have no gaps
    std::vector<DescriptorBinding> bindings;

    explicit DescriptorBuffer(VkPhysicalDeviceDescriptorBufferPropertiesEXT buffer_features,
                              VmaAllocator allocator,
                              VkDevice device)
      : allocator(allocator), device(device), properties_(buffer_features)
    {}

    DescriptorBuffer& update();

    void bind_descriptors(VkCommandBuffer cmd, VkPipelineBindPoint bind_point, VkPipelineLayout pipelineLayout,
              uint32_t set) const;

    DescriptorBuffer& write_image(uint32 binding, VkDescriptorType type,
                                  const VkDescriptorImageInfo& image_info,
                                  uint32 descriptor_index = 0);
    DescriptorBuffer& write_buffer(uint32 binding, VkDescriptorType type,
                                   VkDeviceAddress resource_address, size_t buffer_size);
    DescriptorBuffer& write_image_array(uint32 binding, VkDescriptorType type,
                                              const std::vector<VkDescriptorImageInfo>& image_infos,
                                              uint32_t first_descriptor = 0);

  private:
    VmaAllocator allocator;
    VkDevice device;

    std::vector<DescriptorUpdate> update_infos;
    VkPhysicalDeviceDescriptorBufferPropertiesEXT properties_;

    size_t MapDescriptorSize(VkDescriptorType type) const;
  };
}  // namespace gestalt