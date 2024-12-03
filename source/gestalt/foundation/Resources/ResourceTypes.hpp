#pragma once
#include <cassert>
#include <memory>
#include <variant>
#include <filesystem>

#include "common.hpp"
#include "VulkanTypes.hpp"
#include "VulkanCheck.hpp"
#include <Resources/TextureType.hpp>
#include "Interface/IGpu.hpp"

#include "Descriptor/DescriptorBinding.hpp"
#include "Descriptor/DescriptorUpdate.hpp"

namespace gestalt::foundation {

  struct ResourceTemplate {
    std::string name;

    explicit ResourceTemplate(std::string name) : name(std::move(name)) {}

    virtual ~ResourceTemplate() = default;
  };

  enum class ImageType { kImage2D, kImage3D, kCubeMap };

  // scaled from window/screen resolution
  struct RelativeImageSize {
    float32 scale{1.0f};

    explicit RelativeImageSize(const float32 scale = 1.0f) : scale(scale) {
      assert(scale > 0.0f && "Scale must be positive.");
      assert(scale <= 16.0f && "Scale cannot be higher than 16.0.");
    }
  };

  // fixed dimensions
  struct AbsoluteImageSize {
    VkExtent3D extent{0, 0, 0};

    AbsoluteImageSize(const uint32 width, const uint32 height, const uint32 depth = 0) {
      extent = {width, height, depth};
      assert(width > 0 && height > 0 && "Width and height must be positive.");
    }
  };

  struct ImageTemplate final : ResourceTemplate {
    ImageType image_type = ImageType::kImage2D;
    TextureType type = TextureType::kColor;
    std::variant<VkClearValue, std::filesystem::path> initial_value
        = VkClearValue({.color = {0.f, 0.f, 0.f, 1.f}});
    std::variant<RelativeImageSize, AbsoluteImageSize> image_size = RelativeImageSize(1.f);
    VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;

    // Constructor with name and optional customization
    explicit ImageTemplate(std::string name) : ResourceTemplate(std::move(name)) {}

    // Fluent setters for customization
    ImageTemplate& set_image_type(const TextureType texture_type, const VkFormat format) {
      this->type = texture_type;
      this->format = format;
      return *this;
    }

    ImageTemplate& set_initial_value(const VkClearColorValue& clear_value) {
      assert(type == TextureType::kColor && "Clear color only supported for color images.");
      this->initial_value = VkClearValue({.color = clear_value});
      return *this;
    }

    ImageTemplate& set_initial_value(const VkClearDepthStencilValue& clear_value) {
      assert(type == TextureType::kDepth && "Clear depth only supported for depth images.");
      this->initial_value = VkClearValue({.depthStencil = clear_value});
      return *this;
    }

    ImageTemplate& set_initial_value(const std::filesystem::path& path) {
      assert(type == TextureType::kColor && "path only supported for color images.");
      this->initial_value = path;
      return *this;
    }

    ImageTemplate& set_image_size(const float32& relative_size) {
      this->image_size = RelativeImageSize(relative_size);
      return *this;
    }

    ImageTemplate& set_image_size(const uint32 width, const uint32 height, const uint32 depth = 0) {
      this->image_size = AbsoluteImageSize(width, height, depth);
      return *this;
    }

    ImageTemplate build() { return *this; }

  };

    struct BufferTemplate final : ResourceTemplate {
    size_t size;
    VkBufferUsageFlags usage;
    VmaMemoryUsage memory_usage;

    BufferTemplate(std::string name, const size_t size,
                           const VkBufferUsageFlags usage, const VmaMemoryUsage memory_usage)
        : ResourceTemplate(std::move(name)), size(size), usage(usage), memory_usage(memory_usage) {}
  };

  struct ResourceInstance {
    uint64 resource_handle = -1; // todo refactor to return instance handles
    std::string resource_name;
    explicit ResourceInstance(std::string resource_name) : resource_name(std::move(resource_name)) {}
    [[nodiscard]] std::string_view name() const {
      return resource_name; }
  };

  struct AllocatedImage {
    VkImage image_handle = VK_NULL_HANDLE;
    VkImageView image_view = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
  };

  struct ImageInstance : ResourceInstance {
    ImageInstance(ImageTemplate&& image_template, const AllocatedImage& allocated_image,
                  const VkExtent3D extent)
        : ResourceInstance(image_template.name),
          image_template(std::move(image_template)),
          allocated_image(allocated_image),
          extent(extent) {}

    ImageTemplate image_template;
    AllocatedImage allocated_image;
    VkExtent3D extent;
    VkImageLayout current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  };

  struct AllocatedBuffer {
    VkBuffer buffer_handle = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo info;
    VkDeviceAddress address;
  };

  struct BufferInstance : ResourceInstance {
    BufferInstance(BufferTemplate&& buffer_template, const AllocatedBuffer& allocated_buffer)
        : ResourceInstance(buffer_template.name),
          buffer_template(std::move(buffer_template)),
          allocated_buffer(allocated_buffer){}

    // size that was actually allocated, alignment might have been added
    [[nodiscard]] VkDeviceSize get_size() const { return allocated_buffer.info.size; }

    // size that was requested, max size that can be uploaded, used for descriptor buffer bindings
    [[nodiscard]] VkDeviceSize get_requested_size() const { return buffer_template.size; }

    [[nodiscard]] VkDeviceAddress get_address() const { return allocated_buffer.address; }

    [[nodiscard]] VkBuffer get_buffer_handle() const { return allocated_buffer.buffer_handle; }

  private:
    BufferTemplate buffer_template;
    AllocatedBuffer allocated_buffer;
    VkAccessFlags2 current_access = 0;        // Current access flags
    VkPipelineStageFlags2 current_stage = 0;  // Current pipeline stage
  };
    
  class DescriptorBufferInstance {
    VkBuffer buffer_handle_ = VK_NULL_HANDLE;
    VmaAllocation allocation_ = VK_NULL_HANDLE;
    VmaAllocationInfo info_;
    VkDeviceAddress address_;
    VkDeviceSize size_;
    // these flags are required for descriptor buffers:
    VkBufferUsageFlags usage_ = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
                               | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    // Each offset corresponds to one layout binding in GLSL
    // Bindings need to start at 0 and have no gaps
    std::unordered_map<uint32_t, DescriptorBinding> bindings_;

    IGpu* gpu_;

    std::vector<DescriptorUpdate> update_infos_;

    [[nodiscard]] size_t MapDescriptorSize(const VkDescriptorType type) const {
      const auto& properties = gpu_->getDescriptorBufferProperties();
      switch (type) {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
          return properties.uniformBufferDescriptorSize;
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
          return properties.storageBufferDescriptorSize;
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
          return properties.uniformTexelBufferDescriptorSize;
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
          return properties.storageTexelBufferDescriptorSize;
        case VK_DESCRIPTOR_TYPE_SAMPLER:
          return properties.samplerDescriptorSize;
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
          return properties.combinedImageSamplerDescriptorSize;
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
          return properties.sampledImageDescriptorSize;
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
          return properties.storageImageDescriptorSize;
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
          return properties.inputAttachmentDescriptorSize;
        default:
          assert(false && "Invalid descriptor type");
      }
      return 0;
    }

    static size_t aligned_size(const size_t size, const size_t alignment) {
      return (size + alignment - 1) & ~(alignment - 1);
    }

  public:
    explicit DescriptorBufferInstance(
        IGpu* gpu, const std::string_view name,
                                      const VkDescriptorSetLayout descriptor_layout,
                                      const std::vector<uint32_t>& binding_indices)
        : gpu_(gpu) {
      assert(gpu_ && "GPU instance cannot be null.");

      const VkDeviceSize descriptor_buffer_offset_alignment
          = gpu_->getDescriptorBufferProperties().descriptorBufferOffsetAlignment;
      vkGetDescriptorSetLayoutSizeEXT(gpu_->getDevice(), descriptor_layout,
                                      &size_);
      size_
          = aligned_size(size_, descriptor_buffer_offset_alignment);

      VkDeviceSize binding_offset = 0;

      for (uint32 binding_index : binding_indices) {
        vkGetDescriptorSetLayoutBindingOffsetEXT(gpu_->getDevice(), descriptor_layout,
                                                 binding_index, &binding_offset);
        bindings_.emplace(binding_index, DescriptorBinding{
          .descriptor_size = 0,  // Initialize to zero
                                             .binding = binding_index,
                                             .offset = binding_offset,
                                         });
      }

      VkBufferCreateInfo bufferInfo = {};
      bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      bufferInfo.pNext = nullptr;
      bufferInfo.size = size_;
      bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      bufferInfo.usage = usage_;

      VmaAllocationCreateInfo vmaallocInfo = {};
      vmaallocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
      vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
      VK_CHECK(vmaCreateBuffer(gpu_->getAllocator(), &bufferInfo, &vmaallocInfo,
                               &buffer_handle_, &allocation_,
                               &info_));

      gpu_->set_debug_name(std::string(name) + " Descriptor Buffer", VK_OBJECT_TYPE_BUFFER,
                           reinterpret_cast<uint64>(buffer_handle_));

      VkBufferDeviceAddressInfo device_address_info
          = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
      device_address_info.buffer = buffer_handle_;
      address_ = vkGetBufferDeviceAddress(gpu_->getDevice(), &device_address_info);
    }

    void update() {
      vkDeviceWaitIdle(gpu_->getDevice());

      char* descriptor_buf_ptr = nullptr;
      VK_CHECK(vmaMapMemory(gpu_->getAllocator(), allocation_,
                            reinterpret_cast<void**>(&descriptor_buf_ptr)));

      for (const auto& update_info : update_infos_) {
        VkDescriptorGetInfoEXT descriptor_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
            .type = update_info.type,
        };

        if (update_info.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
          descriptor_info.data.pUniformBuffer = &update_info.addr_info;
        } else if (update_info.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
          descriptor_info.data.pStorageBuffer = &update_info.addr_info;
        } else if (update_info.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
          descriptor_info.data.pCombinedImageSampler = &update_info.image_info;
        } else if (update_info.type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
          descriptor_info.data.pStorageImage = &update_info.image_info;
        } else {
          throw std::runtime_error("Unsupported descriptor type");
        }

        auto binding_it = bindings_.find(update_info.binding);
        if (binding_it == bindings_.end()) {
          throw std::runtime_error("Invalid binding index.");
        }

        const VkDeviceSize offset
            = update_info.descriptorIndex * update_info.descriptorSize + binding_it->second.offset;
        vkGetDescriptorEXT(gpu_->getDevice(), &descriptor_info, update_info.descriptorSize,
                           descriptor_buf_ptr + offset);
      }

      vmaUnmapMemory(gpu_->getAllocator(), allocation_);
      update_infos_.clear();
    }

    [[nodiscard]] VkDeviceAddress get_address() const { return address_; }

    [[nodiscard]] VkBufferUsageFlags get_usage() const { return usage_; }

    void bind_descriptors(const VkCommandBuffer cmd, const VkPipelineBindPoint bind_point,
                          const VkPipelineLayout pipeline_layout, const uint32_t set) const {
      std::vector<uint32_t> binding_indices;
      std::vector<VkDeviceSize> offsets;

      for (const auto& [binding_index, descriptor_binding] : bindings_) {
        binding_indices.push_back(binding_index);
        offsets.push_back(descriptor_binding.offset);
      }

      vkCmdSetDescriptorBufferOffsetsEXT(cmd, bind_point, pipeline_layout, set,
                                         static_cast<uint32_t>(binding_indices.size()),
                                         binding_indices.data(), offsets.data());
    }

    DescriptorBufferInstance& write_image(const uint32 binding, const VkDescriptorType type,
                                          VkDescriptorImageInfo&& image_info,
                                          const uint32 descriptor_index = 0) {
      return write_image_array(binding, type, {image_info}, descriptor_index);
    }
    DescriptorBufferInstance& write_buffer(const uint32 binding, const VkDescriptorType type,
                                           const VkDeviceAddress resource_address, const size_t buffer_size, const uint32 descriptor_index = 0) {
      const auto binding_it = bindings_.find(binding);
      if (binding_it == bindings_.end()) {
        throw std::runtime_error("Invalid binding index.");
      }

      DescriptorBinding& descriptor_binding = binding_it->second;

      DescriptorUpdate update_info = {
        .type = type,
        .descriptorSize = MapDescriptorSize(type),
        .binding = binding,
        .descriptorIndex = descriptor_index,
        .addr_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
            .address = resource_address,
            .range = buffer_size,
            .format = VK_FORMAT_UNDEFINED,
        },
    };

      update_infos_.emplace_back(update_info);
      descriptor_binding.descriptor_size = MapDescriptorSize(type);

      return *this;
    }
    DescriptorBufferInstance& write_image_array(
        const uint32 binding, const VkDescriptorType type,
        const std::vector<VkDescriptorImageInfo>& image_infos, const uint32 first_descriptor = 0) {
      auto binding_it = bindings_.find(binding);
      if (binding_it == bindings_.end()) {
        throw std::runtime_error("Invalid binding index.");
        return *this;
      }

      DescriptorBinding& descriptor_binding = binding_it->second;

      size_t descriptor_size = MapDescriptorSize(type);
      for (size_t i = 0; i < image_infos.size(); ++i) {
        DescriptorUpdate update_info = {
            .type = type,
            .descriptorSize = descriptor_size,
            .binding = binding,
            .descriptorIndex = static_cast<uint32_t>(i + first_descriptor),
            .image_info = image_infos[i],  // Store by value
        };

        update_infos_.emplace_back(std::move(update_info));
      }

      descriptor_binding.descriptor_size = descriptor_size * image_infos.size();

      return *this;
    }

  };


}  // namespace gestalt::graphics::fg