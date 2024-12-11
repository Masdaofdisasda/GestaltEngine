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

  enum class ResourceUsage {
    READ,
    WRITE,
    COUNT  // To represent the total number of usage types
  };

  // scaled from window/screen resolution
  struct RelativeImageSize {
    float32 scale{1.0f};

    explicit RelativeImageSize(const float32 scale = 1.0f) : scale(scale) {
      if (scale <= 0.0f || scale > 16.0f) {
        throw std::runtime_error("Scale must be positive and less than 16.0.");
      }
    }
  };

  // fixed dimensions
  struct AbsoluteImageSize {
    VkExtent3D extent{0, 0, 0};

    AbsoluteImageSize(const uint32 width, const uint32 height, const uint32 depth = 0) {
      extent = {width, height, depth};
      if (width <= 0 || height <= 0) {
        throw std::runtime_error("Width and height must be positive.");
      }
    }
  };

  struct ImageTemplate final : ResourceTemplate {
    ImageType image_type = ImageType::kImage2D;
    TextureType type = TextureType::kColor;
    VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;    
    std::variant<VkClearValue, std::filesystem::path> initial_value
        = VkClearValue({.color = {0.f, 0.f, 0.f, 1.f}});
    std::variant<RelativeImageSize, AbsoluteImageSize> image_size = RelativeImageSize(1.f);
    VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;

    // Constructor with name and optional customization
    explicit ImageTemplate(std::string name) : ResourceTemplate(std::move(name)) {}

    // Fluent setters for customization
    ImageTemplate& set_image_type(const TextureType texture_type, const VkFormat format, const ImageType image_type = ImageType::kImage2D) {
      this->type = texture_type;
      this->format = format;
      if (type == TextureType::kDepth) {
        aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
      } else {
        aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
      }
      this->image_type = image_type;
      return *this;
    }

    ImageTemplate& set_initial_value(const VkClearColorValue& clear_value) {
      if (type == TextureType::kDepth) {
        throw std::runtime_error("Clear color only supported for color images.");
      }
      this->initial_value = VkClearValue({.color = clear_value});
      return *this;
    }

    ImageTemplate& set_initial_value(const VkClearDepthStencilValue& clear_value) {
      if (type == TextureType::kColor) {
        throw std::runtime_error("Clear depth only supported for depth images.");
      }
      this->initial_value = VkClearValue({.depthStencil = clear_value});
      return *this;
    }

    ImageTemplate& set_initial_value(const std::filesystem::path& path) {
      if (type == TextureType::kDepth) {
        throw std::runtime_error("path only supported for color images.");
      }
      this->initial_value = path;
      return *this;
    }

    ImageTemplate& set_image_size(const float32& relative_size) {
      this->image_size = RelativeImageSize(relative_size);
      return *this;
    }

    ImageTemplate& set_image_size(const uint32 width, const uint32 height, const uint32 depth = 0) {
      if (depth != 0 && image_type != ImageType::kImage3D) {
        throw std::runtime_error("Depth can only be set for 3D images.");
      }
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

    struct BufferInstance;
  struct ImageInstance;

  struct ResourceVisitor {
    virtual void visit(BufferInstance& buffer, ResourceUsage usage, VkShaderStageFlags shader_stage)
        = 0;
    virtual void visit(ImageInstance& image, ResourceUsage usage, VkShaderStageFlags shader_stage)
        = 0;
    virtual ~ResourceVisitor() = default;
  };

  struct ResourceInstance {
    uint64 resource_handle = -1; // todo refactor to return instance handles
    std::string resource_name;
    explicit ResourceInstance(std::string resource_name) : resource_name(std::move(resource_name)) {}
    [[nodiscard]] std::string_view name() const {
      return resource_name; }

    virtual void accept(ResourceVisitor& visitor, ResourceUsage usage,
                        VkShaderStageFlags shader_stage)
        = 0;
    virtual ~ResourceInstance() = default;
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

    void accept(ResourceVisitor& visitor, const ResourceUsage usage,
                VkShaderStageFlags shader_stage) override {
      visitor.visit(*this, usage, shader_stage);
    }

    [[nodiscard]] VkImage get_image_handle() const { return allocated_image.image_handle; }

    [[nodiscard]] VkImageAspectFlags get_image_aspect() const {
      return image_template.aspect_flags;
    }

    [[nodiscard]] VkImageView get_image_view() const { return allocated_image.image_view; }

    [[nodiscard]] VkExtent3D get_extent() const { return extent; }

    [[nodiscard]] VkImageLayout get_layout() const { return current_layout; }
    void set_layout(const VkImageLayout layout) { current_layout = layout; }
    [[nodiscard]] VkFormat get_format() const { return image_template.format; }

    [[nodiscard]] TextureType get_type() const { return image_template.type; }

    [[nodiscard]] VkAccessFlags2 get_current_access() const { return current_access; }
    void set_current_access(const VkAccessFlags2 access) { current_access = access; }

    [[nodiscard]] VkPipelineStageFlags2 get_current_stage() const { return current_stage; }
    void set_current_stage(const VkPipelineStageFlags2 stage) { current_stage = stage; }

  private:
    ImageTemplate image_template;
    AllocatedImage allocated_image;
    VkExtent3D extent;
    VkImageLayout current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkAccessFlags2 current_access = VK_ACCESS_2_NONE;
    VkPipelineStageFlags2 current_stage = VK_PIPELINE_STAGE_2_NONE;
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
          allocated_buffer(allocated_buffer) {}

    void accept(ResourceVisitor& visitor, const ResourceUsage usage,
                VkShaderStageFlags shader_stage) override {
      visitor.visit(*this, usage, shader_stage);
    }
    // size that was actually allocated, alignment might have been added
    [[nodiscard]] VkDeviceSize get_size() const { return allocated_buffer.info.size; }

    // size that was requested, max size that can be uploaded, used for descriptor buffer bindings
    [[nodiscard]] VkDeviceSize get_requested_size() const { return buffer_template.size; }

    [[nodiscard]] VkDeviceAddress get_address() const { return allocated_buffer.address; }

    [[nodiscard]] VmaAllocation get_allocation() const { return allocated_buffer.allocation; }

    [[nodiscard]] VkBuffer get_buffer_handle() const { return allocated_buffer.buffer_handle; }

    [[nodiscard]] VkBufferUsageFlags get_usage() const { return buffer_template.usage; }

    [[nodiscard]] VkAccessFlags2 get_current_access() const { return current_access; }
    void set_current_access(const VkAccessFlags2 access) { current_access = access; }

    [[nodiscard]] VkPipelineStageFlags2 get_current_stage() const { return current_stage; }
    void set_current_stage(const VkPipelineStageFlags2 stage) { current_stage = stage; }

  private:
    BufferTemplate buffer_template;
    AllocatedBuffer allocated_buffer;
    VkAccessFlags2 current_access = VK_ACCESS_2_NONE;
    VkPipelineStageFlags2 current_stage = VK_PIPELINE_STAGE_2_NONE;
  };
    
  class DescriptorBufferInstance {
    VkBuffer buffer_handle_ = VK_NULL_HANDLE;
    VmaAllocation allocation_ = VK_NULL_HANDLE;
    VmaAllocationInfo info_;
    VkDeviceAddress address_;
    VkDeviceSize layout_size_in_bytes_;
    // these flags are required for descriptor buffers:
    VkBufferUsageFlags usage_ = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT
                                | VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT 
                               | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
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
          throw std::runtime_error("Descriptor type not implemented");
      }
    }

    static size_t aligned_size(const size_t size, const size_t alignment) {
      return (size + alignment - 1) & ~(alignment - 1);
    }

  public:
    explicit DescriptorBufferInstance(IGpu* gpu, const std::string_view name,
                                      const VkDescriptorSetLayout descriptor_layout,
                                      const std::vector<uint32_t>& binding_indices)
        : gpu_(gpu) {
      if (gpu_ == nullptr) {
        throw std::runtime_error("GPU instance cannot be null.");
      }

      const VkDeviceSize descriptor_buffer_offset_alignment
          = gpu_->getDescriptorBufferProperties().descriptorBufferOffsetAlignment;
      vkGetDescriptorSetLayoutSizeEXT(gpu_->getDevice(), descriptor_layout, &layout_size_in_bytes_);
      layout_size_in_bytes_ = aligned_size(layout_size_in_bytes_, descriptor_buffer_offset_alignment);

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

      const VkBufferCreateInfo buffer_info = {
          .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          .pNext = nullptr,
          .size = layout_size_in_bytes_,
          .usage = usage_,
          .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      };

      constexpr VmaAllocationCreateInfo vma_alloc_info = {
          .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
          .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
      };
      VK_CHECK(vmaCreateBuffer(gpu_->getAllocator(), &buffer_info, &vma_alloc_info, &buffer_handle_,
                               &allocation_, &info_));

      gpu_->set_debug_name(name,
                           VK_OBJECT_TYPE_BUFFER,
                           reinterpret_cast<uint64>(buffer_handle_));

      const VkBufferDeviceAddressInfo device_address_info = {
          .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
          .buffer = buffer_handle_,
      };
      address_ = vkGetBufferDeviceAddress(gpu_->getDevice(), &device_address_info);
    }

    void update() {
      vkDeviceWaitIdle(gpu_->getDevice());

      char* descriptor_buf_ptr = nullptr;
      VK_CHECK(vmaMapMemory(gpu_->getAllocator(), allocation_,
                            reinterpret_cast<void**>(&descriptor_buf_ptr)));

      for (const auto& [type, descriptorSize, binding, descriptorIndex, addr_info, image_info] : update_infos_) {
        VkDescriptorGetInfoEXT descriptor_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
            .type = type,
        };

        if (type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
          descriptor_info.data.pUniformBuffer = &addr_info;
        } else if (type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
          descriptor_info.data.pStorageBuffer = &addr_info;
        } else if (type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
          descriptor_info.data.pCombinedImageSampler = &image_info;
        } else if (type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
          descriptor_info.data.pStorageImage = &image_info;
        } else {
          throw std::runtime_error("Unsupported descriptor type");
        }

        auto binding_it = bindings_.find(binding);
        if (binding_it == bindings_.end()) {
          throw std::runtime_error("Invalid binding index.");
        }

        const VkDeviceSize offset
            = descriptorIndex * descriptorSize + binding_it->second.offset;
        vkGetDescriptorEXT(gpu_->getDevice(), &descriptor_info, descriptorSize,
                           descriptor_buf_ptr + offset);
      }

      vmaUnmapMemory(gpu_->getAllocator(), allocation_);
      update_infos_.clear();
    }

    [[nodiscard]] VkDeviceAddress get_address() const { return address_; }

    [[nodiscard]] VkBufferUsageFlags get_usage() const { return usage_; }

    void bind_descriptors(const VkCommandBuffer cmd, const VkPipelineBindPoint bind_point,
                          const VkPipelineLayout pipeline_layout, const uint32 set) const {
      VkDeviceSize buffer_offset = 0;
      for (const auto& [binding_index, binding]: bindings_) {
        for (int i = 0; i < binding.descriptor_count; ++i) {
          buffer_offset = i * binding.descriptor_size;
          vkCmdSetDescriptorBufferOffsetsEXT(cmd, bind_point, pipeline_layout, set, 1, &set,
                                             &buffer_offset);
        }
      }
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

      const size_t descriptor_size = MapDescriptorSize(type);
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