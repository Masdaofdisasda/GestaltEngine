#include "GpuResources.hpp"
#include "GpuTypes.hpp"


namespace gestalt::foundation {
  DescriptorBuffer& DescriptorBuffer::update() {
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

      const VkDeviceSize offset = descriptorIndex * descriptorSize + offsets[binding];
      vkGetDescriptorEXT(device, &descriptor_info, descriptorSize,
                         descriptor_buf_ptr + offset);
    }

    vmaUnmapMemory(allocator, allocation);
    update_infos.clear();

     return *this;
  }

  void DescriptorBuffer::bind(VkCommandBuffer cmd, VkPipelineBindPoint bind_point,
                              VkPipelineLayout pipelineLayout, uint32_t set) const {

      std::vector<VkDescriptorBufferBindingInfoEXT> bufferBindings;
      bufferBindings.reserve(offsets.size());
      for (int i = 0; i < offsets.size(); ++i) {
        bufferBindings.push_back({.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
                                  .address = address,
                                  .usage = usage});
    }
    vkCmdBindDescriptorBuffersEXT(cmd, bufferBindings.size(), bufferBindings.data());

    for (int i = 0; i < offsets.size(); ++i) {
      uint32 bufferIndex = i;
      VkDeviceSize offset = offsets[i];
      vkCmdSetDescriptorBufferOffsetsEXT(cmd, bind_point, pipelineLayout, set, 1, &bufferIndex,
                                         &offset);
    }
  }


  size_t DescriptorBuffer::MapDescriptorSize(const VkDescriptorType type) const {
    switch (type) {
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        return properties_.uniformBufferDescriptorSize;
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
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
        return 0;
    }
  }

  DescriptorBuffer& DescriptorBuffer::write_buffer(
      uint32 binding,
                                                               VkDescriptorType type,
                                                               VkDeviceAddress resource_address,
                                                               size_t descriptor_range,
                                                               uint32 descriptor_index) {
    return write_buffer_array(binding, type, resource_address, descriptor_range, 1,
                              descriptor_index);
  }

  DescriptorBuffer& DescriptorBuffer::write_buffer_array(uint32 binding, VkDescriptorType type,
                                                         VkDeviceAddress resource_address,
                                                         size_t descriptor_range,
                                                         uint32 descriptor_count,
                                                         uint32 first_descriptor) {
    update_infos.reserve(descriptor_count);

    for (uint32 descriptor_index = first_descriptor;
         descriptor_index < first_descriptor + descriptor_count; ++descriptor_index) {
      VkDescriptorAddressInfoEXT addr_info
          = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
             .address = resource_address,
             .range = descriptor_range,
             .format = VK_FORMAT_UNDEFINED};

      DescriptorUpdate descriptor_update{.type = type,
                                         .descriptorSize = MapDescriptorSize(type),
                                         .binding = binding,
                                         .descriptorIndex = descriptor_index,
                                         .addr_info = addr_info};

      update_infos.emplace_back(descriptor_update);
    }
    return *this;
  }

  DescriptorBuffer& DescriptorBuffer::write_image_array(
      uint32 binding, VkDescriptorType type, const std::vector<VkDescriptorImageInfo>& image_infos,
      uint32 first_descriptor) {

    update_infos.reserve(image_infos.size());
    for (uint32 descriptor = first_descriptor; descriptor < image_infos.size(); ++descriptor) {
      DescriptorUpdate update_info{.type = type,
                               .descriptorSize = MapDescriptorSize(type),
                               .binding = binding,
                               .descriptorIndex = descriptor,
                               .image_info = image_infos[descriptor]};
      update_infos.emplace_back(update_info);
    }

    return *this;
  }

  DescriptorBuffer& DescriptorBuffer::write_image(
      uint32 binding, VkDescriptorType type, const VkDescriptorImageInfo& image_info,
      uint32 descriptor_index) {
    return write_image_array(binding, type, {image_info}, descriptor_index);
  }
}  // namespace gestalt::foundation
