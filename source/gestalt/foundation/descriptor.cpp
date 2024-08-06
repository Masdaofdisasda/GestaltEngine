#include "GpuResources.hpp"
#include "GpuTypes.hpp"


namespace gestalt::foundation {
  DescriptorBuffer& DescriptorBuffer::update() {

    vkDeviceWaitIdle(device);
    char* descriptor_buf_ptr;
    VK_CHECK(vmaMapMemory(allocator, allocation, reinterpret_cast<void**>(&descriptor_buf_ptr)));

    for (auto& [descriptorType, descriptorSize, binding, descriptorIndex, addr_info, img_info] : update_infos) {
      VkDescriptorGetInfoEXT descriptor_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
        .type = descriptorType,
      };

      if (descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
        descriptor_info.data.pUniformBuffer = &addr_info;
      } else if (descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
        descriptor_info.data.pStorageBuffer = &addr_info;
      } else if (descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
        descriptor_info.data.pCombinedImageSampler = &img_info;
      } else {
        assert(false && "Invalid descriptor type");
      }

      const VkDeviceSize offset = descriptorIndex * descriptorSize + bindings[binding].offset;
      vkGetDescriptorEXT(device, &descriptor_info, descriptorSize,
                         descriptor_buf_ptr + offset);
    }

    vmaUnmapMemory(allocator, allocation);
    update_infos.clear();

     return *this;
  }

  void DescriptorBuffer::bind_descriptors(VkCommandBuffer cmd, VkPipelineBindPoint bind_point,
                                          VkPipelineLayout pipelineLayout, uint32_t set) const {
    VkDeviceSize buffer_offset = 0;
    for (const auto& [descriptorSize, descriptorCount, binding, offset] : bindings) {
      for (int i = 0; i < descriptorCount; ++i) {
        buffer_offset = i * descriptorSize;
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, bind_point, pipelineLayout, set, 1, &set,
                                           &buffer_offset);
      }
    }
  }


  size_t DescriptorBuffer::MapDescriptorSize(const VkDescriptorType type) const {
    switch (type) {
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        return properties_.uniformBufferDescriptorSize;
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        return properties_.storageBufferDescriptorSize;
      case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        return properties_.uniformTexelBufferDescriptorSize;
      case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        return properties_.storageTexelBufferDescriptorSize;
      case VK_DESCRIPTOR_TYPE_SAMPLER:
        return properties_.samplerDescriptorSize;
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        return properties_.combinedImageSamplerDescriptorSize;
      case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        return properties_.sampledImageDescriptorSize;
      case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        return properties_.storageImageDescriptorSize;
      case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        return properties_.inputAttachmentDescriptorSize;
      default:
        assert( false && "Invalid descriptor type");
    }
  }

  DescriptorBuffer& DescriptorBuffer::write_buffer(uint32 binding, VkDescriptorType type,
                                                         VkDeviceAddress resource_address,
                                                         size_t buffer_size) {
    VkDescriptorAddressInfoEXT addr_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
                                            .address = resource_address,
                                            .range = buffer_size,
                                            .format = VK_FORMAT_UNDEFINED};

    update_infos.emplace_back(DescriptorUpdate{.type = type,
                                               .descriptorSize = MapDescriptorSize(type),
                                               .binding = binding,
                                               .addr_info = addr_info});
    bindings.at(binding).descriptorSize = MapDescriptorSize(type);
    return *this;
  }

  DescriptorBuffer& DescriptorBuffer::write_image_array(
      uint32 binding, VkDescriptorType type, const std::vector<VkDescriptorImageInfo>& image_infos,
      uint32 first_descriptor) {

    update_infos.reserve(image_infos.size());
    for (uint32 i = 0; i < image_infos.size();
         ++i) {
      DescriptorUpdate update_info{.type = type,
                               .descriptorSize = MapDescriptorSize(type),
                               .binding = binding,
                               .descriptorIndex = i + first_descriptor,
                               .image_info = image_infos[i]};
      update_infos.emplace_back(update_info);
    }
    bindings.at(binding).descriptorSize = MapDescriptorSize(type) * image_infos.size();
    return *this;
  }

  DescriptorBuffer& DescriptorBuffer::write_image(
      uint32 binding, VkDescriptorType type, const VkDescriptorImageInfo& image_info,
      uint32 descriptor_index) {
    return write_image_array(binding, type, {image_info}, descriptor_index);
  }
}  // namespace gestalt::foundation
