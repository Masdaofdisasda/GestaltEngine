#pragma once 
#include <VulkanTypes.hpp>

#include "Pipeline.hpp"
#include "PushDescriptor.hpp"
#include "Resources/ResourceTypes.hpp"

namespace gestalt::graphics {


  struct ResourceComponentBindings {
    std::map<uint32, std::shared_ptr<ImageInstance>> color_attachments;
    std::shared_ptr<ImageInstance> depth_attachment;

    //TODO
    std::vector<ResourceBinding<ImageArrayInstance>> image_array_bindings;
    std::vector<ResourceBinding<ImageInstance>> image_bindings;
    std::vector<ResourceBinding<BufferInstance>> buffer_bindings;

    PushDescriptor push_descriptor;

    ResourceComponentBindings& add_attachment(const std::shared_ptr<ImageInstance>& image,
                                              uint32 attachment_index = 0) {
      if (!image) {
        throw std::invalid_argument("Image cannot be null");
      }

      switch (image->get_type()) {
        case TextureType::kColor:
          color_attachments.emplace(attachment_index, image);
          break;
        case TextureType::kDepth:
          if (attachment_index != 0) {
            throw std::runtime_error("Depth attachment index must be 0");
          }
          depth_attachment = image;
          break;
        default:
          throw std::runtime_error("Unsupported attachment type");
      }

      // use VK_DESCRIPTOR_TYPE_MAX_ENUM so that the descriptor class knows that it can't use the
      // descriptor type
      image_bindings.push_back(
          {.resource= image,
           .info= {.set_index= 0, .binding_index= 0, .descriptor_type= VK_DESCRIPTOR_TYPE_MAX_ENUM, .shader_stages=
                   VK_SHADER_STAGE_FRAGMENT_BIT, .descriptor_count= 1, .sampler = nullptr},
           .usage= ResourceUsage::WRITE});
      return *this;
    }

    // Add an image binding
    ResourceComponentBindings& add_binding(uint32 set_index, uint32 binding_index,
                                           const std::shared_ptr<ImageInstance>& image,
                                           VkSampler sampler, ResourceUsage usage,
                                           VkDescriptorType descriptor_type,
                                           VkShaderStageFlags shader_stages,
                                           uint32 descriptor_count = 1) {
      if (!image) {
        throw std::invalid_argument("Image cannot be null");
      }

      image_bindings.push_back(
          {image,
           {set_index, binding_index, descriptor_type, shader_stages, descriptor_count, sampler},
           usage});
      return *this;
    }

    ResourceComponentBindings& add_binding(uint32 set_index, uint32 binding_index,
                                           std::shared_ptr<ImageArrayInstance> images,
                                           ResourceUsage usage, VkDescriptorType descriptor_type,
                                           VkShaderStageFlags shader_stages) {
      if (!images) {
        throw std::invalid_argument("Image array cannot be null");
      }

      image_array_bindings.push_back(
          {images, {set_index, binding_index, descriptor_type, shader_stages, static_cast<uint32>(images->get_max_images()), nullptr}, usage}
          );

      return *this;
    }

    // Add a buffer binding
    ResourceComponentBindings& add_binding(uint32 set_index, uint32 binding_index,
                                           const std::shared_ptr<BufferInstance>& buffer,
                                           ResourceUsage usage, VkDescriptorType descriptor_type,
                                           VkShaderStageFlags shader_stages) {
      if (!buffer) {
        throw std::invalid_argument("Buffer cannot be null");
      }

      buffer_bindings.push_back(
          {buffer, {set_index, binding_index, descriptor_type, shader_stages, 1}, usage});
      return *this;
    }

    // Add push constants
    ResourceComponentBindings& add_push_constant(uint32_t size, VkShaderStageFlags stage_flags) {
      push_descriptor = PushDescriptor(size, stage_flags);
      return *this;
    }
  };

  class ResourceComponent {
  public:
    explicit ResourceComponent(ResourceComponentBindings&& bindings) : data_(std::move(bindings)) {}

    // Retrieve all resources based on usage
    [[nodiscard]] std::vector<ResourceBinding<ResourceInstance>> get_resources(
        ResourceUsage usage) const {
      std::vector<ResourceBinding<ResourceInstance>> result;
      append_bindings(result, data_.image_bindings, usage);
      append_bindings(result, data_.buffer_bindings, usage);
      return result;
    }

    [[nodiscard]] ResourceBinding<BufferInstance> get_buffer_binding(
        const uint32 set_index, const uint32 binding_index) const {
      for (const auto& resource : data_.buffer_bindings) {
        if (resource.info.set_index == set_index && resource.info.binding_index == binding_index) {
          return resource;
        }
      }
      throw std::runtime_error("Resource binding not found");
    }

    [[nodiscard]] ResourceBinding<ImageInstance> get_image_binding(
        const uint32 set_index, const uint32 binding_index) const {
      for (const auto& resource : data_.image_bindings) {
        if (resource.info.set_index == set_index && resource.info.binding_index == binding_index) {
          return resource;
        }
      }
      throw std::runtime_error("Resource binding not found");
    }

    // Get image or buffer bindings as spans
    [[nodiscard]] std::span<const ResourceBinding<ImageInstance>> get_image_bindings() const {
      return data_.image_bindings;
    }

    [[nodiscard]] std::span<const ResourceBinding<BufferInstance>> get_buffer_bindings() const {
      return data_.buffer_bindings;
    }

    [[nodiscard]] std::span<const ResourceBinding<ImageArrayInstance>>
    get_image_array_bindings() const {
      return data_.image_array_bindings;
    }

    // Retrieve specific attachments
    [[nodiscard]] const auto& get_color_attachments() const { return data_.color_attachments; }
    [[nodiscard]] const auto& get_depth_attachment() const { return data_.depth_attachment; }

    // Get individual bindings
    [[nodiscard]] VkPushConstantRange get_push_constant_range() const {
      return data_.push_descriptor.get_push_constant_range();
    }

  private:
    ResourceComponentBindings data_;

    // Helper function to append bindings with type conversion
    template <typename T>
    void append_bindings(std::vector<ResourceBinding<ResourceInstance>>& result,
                         const std::vector<ResourceBinding<T>>& bindings,
                         ResourceUsage usage) const {
      for (const auto& binding : bindings) {
        if (binding.usage == usage) {
          result.push_back({std::static_pointer_cast<ResourceInstance>(binding.resource),
                            binding.info, binding.usage});
        }
      }
    }
  };

}  // namespace gestalt