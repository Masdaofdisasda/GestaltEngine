#pragma once
#include <cassert>
#include <cstring>
#include <filesystem>
#include <memory>
#include <variant>

#include <Resources/TextureType.hpp>
#include "VulkanCheck.hpp"
#include "VulkanCore.hpp"
#include "common.hpp"

#include "AccelerationStructure.hpp"
#include "Interface/IGpu.hpp"

#include "Descriptor/DescriptorBinding.hpp"
#include "Descriptor/DescriptorUpdate.hpp"
#include "Material/Material.hpp"

namespace gestalt::foundation {

  class ResourceTemplate {
    std::string name_;

  public:
    ResourceTemplate(const ResourceTemplate&) = delete;
    ResourceTemplate& operator=(const ResourceTemplate&) = delete;

    ResourceTemplate(ResourceTemplate&&) noexcept = default;
    ResourceTemplate& operator=(ResourceTemplate&&) noexcept = default;

    explicit ResourceTemplate(std::string name)
      : name_(std::move(name)) {
    }

    [[nodiscard]] std::string get_name() const { return name_; }

    virtual ~ResourceTemplate() = default;
  };

  enum class ImageType { kImage2D, kImage3D, kCubeMap };

  enum class ResourceUsage {
    READ,
    WRITE,
    COUNT  // To represent the total number of usage types
  };

  // scaled from window/screen resolution
  class RelativeImageSize {
    float32 scale_{1.0f};

  public:
    explicit RelativeImageSize(const float32 scale = 1.0f) : scale_(scale) {
      if (scale <= 0.0f || scale > 16.0f) {
        throw std::runtime_error("Scale must be positive and less than 16.0.");
      }
    }
    [[nodiscard]] float32 scale() const { return scale_; }
  };

  // fixed dimensions
  class AbsoluteImageSize {
    VkExtent3D extent_{0, 0, 0};

  public:
    AbsoluteImageSize(const uint32 width, const uint32 height, const uint32 depth = 0) {
      extent_ = {width, height, depth};
      if (width <= 0 || height <= 0) {
        throw std::runtime_error("Width and height must be positive.");
      }
    }
    [[nodiscard]] VkExtent3D extent() const { return extent_; }
  };

  class ImageTemplate final : public ResourceTemplate {
    ImageType image_type_ = ImageType::kImage2D;
    TextureType type_ = TextureType::kColor;
    VkImageAspectFlags aspect_flags_ = VK_IMAGE_ASPECT_COLOR_BIT;
    std::variant<VkClearValue, std::filesystem::path, std::vector<unsigned char>> initial_value_
        = VkClearValue({.color = {0.f, 0.f, 0.f, 1.f}});
    std::variant<RelativeImageSize, AbsoluteImageSize> image_size_ = RelativeImageSize(1.f);
    VkFormat format_ = VK_FORMAT_R16G16B16A16_SFLOAT;
    bool has_mipmap_ = false;

  public:
    explicit ImageTemplate(std::string name)
      : ResourceTemplate(std::move(name)) {
    }

    ImageTemplate(const ImageTemplate&) = delete;
    ImageTemplate& operator=(const ImageTemplate&) = delete;

    ImageTemplate(ImageTemplate&&) noexcept = default;
    ImageTemplate& operator=(ImageTemplate&&) noexcept = default;

    ~ImageTemplate() override = default;

    ImageTemplate& set_image_type(const TextureType texture_type, const VkFormat format,
                                  const ImageType image_type = ImageType::kImage2D) {
      this->type_ = texture_type;
      this->format_ = format;
      if (type_ == TextureType::kDepth) {
        aspect_flags_ = VK_IMAGE_ASPECT_DEPTH_BIT;
      } else {
        aspect_flags_ = VK_IMAGE_ASPECT_COLOR_BIT;
      }
      this->image_type_ = image_type;
      return *this;
    }

    ImageTemplate& set_initial_value(const VkClearColorValue& clear_value) {
      if (type_ == TextureType::kDepth) {
        throw std::runtime_error("Clear color only supported for color images.");
      }
      this->initial_value_ = VkClearValue({.color = clear_value});
      return *this;
    }

    ImageTemplate& set_initial_value(const VkClearDepthStencilValue& clear_value) {
      if (type_ == TextureType::kColor) {
        throw std::runtime_error("Clear depth only supported for depth images.");
      }
      this->initial_value_ = VkClearValue({.depthStencil = clear_value});
      return *this;
    }

    ImageTemplate& set_initial_value(const std::filesystem::path& path) {
      if (type_ == TextureType::kDepth) {
        throw std::runtime_error("path only supported for color images.");
      }
      this->initial_value_ = path;
      return *this;
    }

    ImageTemplate& set_initial_value(const unsigned char* data, const size_t size) {
      initial_value_ = std::vector(data, data + size);
      return *this;
    }

    ImageTemplate& set_image_size(const float32& relative_size) {
      this->image_size_ = RelativeImageSize(relative_size);
      return *this;
    }

    ImageTemplate& set_image_size(const uint32 width, const uint32 height, const uint32 depth = 0) {
      if (depth != 0 && image_type_ != ImageType::kImage3D) {
        throw std::runtime_error("Depth can only be set for 3D images.");
      }
      this->image_size_ = AbsoluteImageSize(width, height, depth);
      return *this;
    }

    ImageTemplate& set_has_mipmap(const bool has_mipmap) {
      has_mipmap_ = has_mipmap;
      return *this;
    }

    ImageTemplate build() { return std::move(*this); }

    [[nodiscard]] ImageType image_type() const { return image_type_; }
    [[nodiscard]] TextureType type() const { return type_; }
    [[nodiscard]] VkImageAspectFlags aspect_flags() const { return aspect_flags_; }
    [[nodiscard]] VkFormat format() const { return format_; }

    [[nodiscard]] std::variant<VkClearValue, std::filesystem::path, std::vector<unsigned char>>
    initial_value() const { return initial_value_; }

    [[nodiscard]] std::variant<RelativeImageSize, AbsoluteImageSize> image_size() const {
      return image_size_;
    }
    [[nodiscard]] bool has_mipmap() const { return has_mipmap_; }

  };

  class BufferTemplate final : public ResourceTemplate {
    size_t size_;                              // Size of the buffer
    VkBufferUsageFlags usage_;                 // Vulkan buffer usage flags
    VmaMemoryUsage memory_usage_;              // Memory usage flags
    VmaAllocationCreateFlags alloc_flags_;     // Allocation creation flags
    VkMemoryPropertyFlags memory_properties_;  // Explicit memory property flags
    VkBufferCreateFlags buffer_create_flags_;  // Additional Vulkan buffer creation flags

  public:

    BufferTemplate(std::string name, const size_t size, const VkBufferUsageFlags usage,
                   const VmaAllocationCreateFlags alloc_flags,
                   const VmaMemoryUsage memory_usage = VMA_MEMORY_USAGE_AUTO,
                   const VkMemoryPropertyFlags memory_properties = 0,
                   const VkBufferCreateFlags buffer_create_flags = 0)
        : ResourceTemplate(std::move(name)),
          size_(size),
          usage_(usage),
          memory_usage_(memory_usage),
          alloc_flags_(alloc_flags),
          memory_properties_(memory_properties),
          buffer_create_flags_(buffer_create_flags) {}

    BufferTemplate(const BufferTemplate&) = delete;
    BufferTemplate& operator=(const BufferTemplate&) = delete;

    BufferTemplate(BufferTemplate&&) noexcept = default;
    BufferTemplate& operator=(BufferTemplate&&) noexcept = default;

    ~BufferTemplate() override = default;

    [[nodiscard]] size_t get_size() const { return size_; }
    [[nodiscard]] VkBufferUsageFlags get_usage() const { return usage_; }
    [[nodiscard]] VmaMemoryUsage get_memory_usage() const { return memory_usage_; }
    [[nodiscard]] VmaAllocationCreateFlags get_alloc_flags() const { return alloc_flags_; }
    [[nodiscard]] VkMemoryPropertyFlags get_memory_properties() const { return memory_properties_; }
    [[nodiscard]] VkBufferCreateFlags get_buffer_create_flags() const {
      return buffer_create_flags_;
    }
  };

  class SamplerTemplate final : public ResourceTemplate {

    VkSamplerCreateInfo create_info_;

  public:
    SamplerTemplate(std::string name, const VkFilter mag_filter, const VkFilter min_filter,
                    const VkSamplerMipmapMode mipmap_mode,
                    const VkSamplerAddressMode address_mode_u,
                    const VkSamplerAddressMode address_mode_v,
                    const VkSamplerAddressMode address_mode_w, const VkBool32 anisotropy_enable,
                    const float32 mip_lod_bias = 0.0f, const float32 min_lod = 0.0f,
                    const float32 max_lod = VK_LOD_CLAMP_NONE, const float32 max_anisotropy = 16.0f,
                    VkBool32 compare_enable = VK_FALSE,
                    const VkCompareOp compare_op = VK_COMPARE_OP_NEVER,
                    const VkBorderColor border_color = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK)
        : ResourceTemplate(std::move(name)) {
      create_info_ = {
          .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0,
          .magFilter = mag_filter,
          .minFilter = min_filter,
          .mipmapMode = mipmap_mode,
          .addressModeU = address_mode_u,
          .addressModeV = address_mode_v,
          .addressModeW = address_mode_w,
          .mipLodBias = mip_lod_bias,
          .anisotropyEnable = anisotropy_enable,
          .maxAnisotropy = max_anisotropy,
          .compareEnable = compare_enable,
          .compareOp = compare_op,
          .minLod = min_lod,
          .maxLod = max_lod,
          .borderColor = border_color,
          .unnormalizedCoordinates = VK_FALSE,
      };
    }

    SamplerTemplate(const SamplerTemplate&) = delete;
    SamplerTemplate& operator=(const SamplerTemplate&) = delete;

    SamplerTemplate(SamplerTemplate&&) noexcept = default;
    SamplerTemplate& operator=(SamplerTemplate&&) noexcept = default;

    [[nodiscard]] VkSamplerCreateInfo get_create_info() const { return create_info_; }

    ~SamplerTemplate() override = default;
  };

  class BufferInstance;
  class ImageInstance;
  class ImageArrayInstance;
  class AccelerationStructureInstance;

  struct ResourceVisitor {
    virtual void visit(BufferInstance& buffer, ResourceUsage usage, VkShaderStageFlags shader_stage) = 0;
    virtual void visit(ImageInstance& image, ResourceUsage usage, VkShaderStageFlags shader_stage) = 0;
    virtual void visit(ImageArrayInstance& images, ResourceUsage usage, VkShaderStageFlags shader_stage) = 0;
    virtual void visit(AccelerationStructureInstance& images, ResourceUsage usage, VkShaderStageFlags shader_stage) = 0;
    virtual ~ResourceVisitor() = default;
  };

  class ResourceInstance {
    uint64 resource_handle_ = -1; // todo refactor to return instance handles
    std::string resource_name_;

  public:
    explicit ResourceInstance(std::string resource_name) : resource_name_(std::move(resource_name)) {}
    [[nodiscard]] std::string_view name() const {
      return resource_name_; }

    [[nodiscard]] uint64 handle() const { return resource_handle_; }
    void set_handle(const uint64 handle) { resource_handle_ = handle; }

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

  class AccelerationStructureInstance final : public ResourceInstance {
  public:
    explicit AccelerationStructureInstance(const std::shared_ptr<AccelerationStructure>& accelerationStructure)
    : ResourceInstance("TLAS")
    , mAccelerationStructure(accelerationStructure) {
    }

    void accept(ResourceVisitor& visitor, ResourceUsage usage, VkShaderStageFlags shader_stage) override {
      visitor.visit(*this, usage, shader_stage);
    }

    const VkAccelerationStructureKHR& getHandle() const { return mAccelerationStructure->acceleration_structure; }

    const VkDeviceAddress& getAddress() const { return mAccelerationStructure->address; }

    ~AccelerationStructureInstance() override = default;

  private:
    std::shared_ptr<AccelerationStructure> mAccelerationStructure;
  };

  class ImageInstance final : public ResourceInstance {
    ImageTemplate image_template;
    AllocatedImage allocated_image;
    VkExtent3D extent;
    VkImageLayout current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkAccessFlags2 current_access = VK_ACCESS_2_NONE;
    VkPipelineStageFlags2 current_stage = VK_PIPELINE_STAGE_2_NONE;

  public:
    ImageInstance(ImageTemplate&& image_template, const AllocatedImage& allocated_image,
                  const VkExtent3D extent)
      : ResourceInstance(image_template.get_name()),
        image_template(std::move(image_template)),
        allocated_image(allocated_image),
        extent(extent) {}

    ImageInstance(const ImageInstance&) = delete;
    ImageInstance& operator=(const ImageInstance&) = delete;

    ImageInstance(ImageInstance&&) noexcept = default;
    ImageInstance& operator=(ImageInstance&&) noexcept = default;  

    void accept(ResourceVisitor& visitor, const ResourceUsage usage,
                VkShaderStageFlags shader_stage) override {
      visitor.visit(*this, usage, shader_stage);
    }

    [[nodiscard]] VkImage get_image_handle() const { return allocated_image.image_handle; }

    [[nodiscard]] VkImageAspectFlags get_image_aspect() const {
      return image_template.aspect_flags();
    }

    [[nodiscard]] VkImageView get_image_view() const { return allocated_image.image_view; }

    [[nodiscard]] VkExtent3D get_extent() const { return extent; }

    [[nodiscard]] VkImageLayout get_layout() const { return current_layout; }
    void set_layout(const VkImageLayout layout) { current_layout = layout; }
    [[nodiscard]] VkFormat get_format() const { return image_template.format(); }

    [[nodiscard]] TextureType get_type() const { return image_template.type(); }

    [[nodiscard]] VkAccessFlags2 get_current_access() const { return current_access; }
    void set_current_access(const VkAccessFlags2 access) { current_access = access; }

    [[nodiscard]] VkPipelineStageFlags2 get_current_stage() const { return current_stage; }
    void set_current_stage(const VkPipelineStageFlags2 stage) { current_stage = stage; }
  };

  class ImageArrayInstance final : public ResourceInstance {
    std::function<std::vector<Material>()> materials_;
    size_t max_images_;
    size_t previous_size_ = 0;

  public:
    ImageArrayInstance(std::string name, std::function<std::vector<Material>()> materials,
                       const size_t max_images)
      : ResourceInstance(std::move(name)),
        materials_(std::move(materials)),
        max_images_(max_images) {
    }

    void accept(ResourceVisitor& visitor, const ResourceUsage usage,
                const VkShaderStageFlags shader_stage) override {
      visitor.visit(*this, usage, shader_stage);
    }

    [[nodiscard]] bool should_rebuild_descriptors() {
      if (previous_size_ != materials_().size()) {
        previous_size_ = materials_().size();
        return true;
      }
      return false;
    }

    [[nodiscard]] std::vector<Material> get_materials() const {
      return materials_();
    }

    [[nodiscard]] size_t get_max_images() const { return max_images_; }

  };

  struct AllocatedBuffer {
    VkBuffer buffer_handle = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo info;
    VkDeviceAddress address;
  };

  class BufferInstance final : public ResourceInstance {
    BufferTemplate buffer_template_;
    AllocatedBuffer allocated_buffer_;
    VkAccessFlags2 current_access_ = VK_ACCESS_2_NONE;
    VkPipelineStageFlags2 current_stage_ = VK_PIPELINE_STAGE_2_NONE;

  public:
    BufferInstance(BufferTemplate&& buffer_template, const AllocatedBuffer& allocated_buffer)
      : ResourceInstance(buffer_template.get_name()),
        buffer_template_(std::move(buffer_template)),
        allocated_buffer_(allocated_buffer) {}


    BufferInstance(const BufferInstance&) = delete;
    BufferInstance& operator=(const BufferInstance&) = delete;

    BufferInstance(BufferInstance&&) noexcept = default;
    BufferInstance& operator=(BufferInstance&&) noexcept = default;  

    void accept(ResourceVisitor& visitor, const ResourceUsage usage,
                VkShaderStageFlags shader_stage) override {
      visitor.visit(*this, usage, shader_stage);
    }
    // size that was actually allocated, alignment might have been added
    [[nodiscard]] VkDeviceSize get_size() const { return allocated_buffer_.info.size; }

    // size that was requested, max size that can be uploaded, used for descriptor buffer bindings
    [[nodiscard]] VkDeviceSize get_requested_size() const { return buffer_template_.get_size(); }

    [[nodiscard]] VkDeviceAddress get_address() const { return allocated_buffer_.address; }

    [[nodiscard]] VmaAllocation get_allocation() const { return allocated_buffer_.allocation; }

    [[nodiscard]] VkBuffer get_buffer_handle() const { return allocated_buffer_.buffer_handle; }

    [[nodiscard]] VkBufferUsageFlags get_usage() const { return buffer_template_.get_usage(); }

    [[nodiscard]] VkAccessFlags2 get_current_access() const { return current_access_; }
    void set_current_access(const VkAccessFlags2 access) { current_access_ = access; }

    [[nodiscard]] VkPipelineStageFlags2 get_current_stage() const { return current_stage_; }
    void set_current_stage(const VkPipelineStageFlags2 stage) { current_stage_ = stage; }

    void copy_to_mapped(const VmaAllocator allocator, void const* data, const size_t size ) const {
      void* mapped_data;
      VK_CHECK(vmaMapMemory(allocator, get_allocation(), &mapped_data));
      memcpy(mapped_data, data, size);
      vmaUnmapMemory(allocator, get_allocation());
    }
  };

  
  class SamplerInstance final : public ResourceInstance {
    SamplerTemplate sampler_template_;
    VkSampler sampler_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;

  public:
    SamplerInstance(SamplerTemplate&& sampler_template, const VkDevice device)
        : ResourceInstance(sampler_template.get_name()),
          sampler_template_(std::move(sampler_template)), device_(device) {
      const auto create_info = sampler_template_.get_create_info();
      VK_CHECK(vkCreateSampler(device_, &create_info, nullptr, &sampler_));
    }

    SamplerInstance(const SamplerInstance&) = delete;
    SamplerInstance& operator=(const SamplerInstance&) = delete;

    SamplerInstance(SamplerInstance&&) noexcept = default;
    SamplerInstance& operator=(SamplerInstance&&) noexcept = default;

    ~SamplerInstance() override {
      /*
      if (sampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_, sampler_, nullptr);
        sampler_ = VK_NULL_HANDLE;
      }*/
    }

    void accept(ResourceVisitor& visitor, const ResourceUsage usage,
                const VkShaderStageFlags shader_stage) override {
      // do nothing
    }

    [[nodiscard]] VkSampler get_sampler_handle() const { return sampler_; }
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
    std::string name_;

    std::vector<DescriptorUpdate> update_infos_;

    [[nodiscard]] size_t map_descriptor_size(const VkDescriptorType type) const {
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
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
          return properties.accelerationStructureDescriptorSize;
        default:
          throw std::runtime_error("Descriptor type not implemented");
      }
    }

    static size_t aligned_size(const size_t size, const size_t alignment) {
      return (size + alignment - 1) & ~(alignment - 1);
    }

  public:
    explicit DescriptorBufferInstance(IGpu* gpu, std::string name,
                                      const VkDescriptorSetLayout descriptor_layout,
                                      const std::vector<uint32_t>& binding_indices)
        : gpu_(gpu), name_(std::move(name)) {
      if (gpu_ == nullptr) {
        throw std::runtime_error("GPU instance cannot be null.");
      }

      const VkDeviceSize descriptor_buffer_offset_alignment
          = gpu_->getDescriptorBufferProperties().descriptorBufferOffsetAlignment;
      vkGetDescriptorSetLayoutSizeEXT(gpu_->getDevice(), descriptor_layout, &layout_size_in_bytes_);
      layout_size_in_bytes_
          = aligned_size(layout_size_in_bytes_, descriptor_buffer_offset_alignment);

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

      gpu_->set_debug_name(name_, VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64>(buffer_handle_));

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

      for (const auto& update : update_infos_) {
        VkDescriptorGetInfoEXT descriptor_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
            .type = update.type,
        };

        if (update.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
          descriptor_info.data.pUniformBuffer = &update.addr_info;
        } else if (update.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
          descriptor_info.data.pStorageBuffer = &update.addr_info;
        } else if (update.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
          descriptor_info.data.pCombinedImageSampler = &update.image_info;
        } else if (update.type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
          descriptor_info.data.pStorageImage = &update.image_info;
        } else if (update.type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR) {
          descriptor_info.data.accelerationStructure = update.tlas_address;
        } else {
          throw std::runtime_error("Unsupported descriptor type");
        }

        auto binding_it = bindings_.find(update.binding);
        if (binding_it == bindings_.end()) {
          throw std::runtime_error("Invalid binding index.");
        }

        const VkDeviceSize offset = update.descriptorIndex * update.descriptorSize + binding_it->second.offset;
        vkGetDescriptorEXT(gpu_->getDevice(), &descriptor_info, update.descriptorSize,
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
      for (const auto& [binding_index, binding] : bindings_) {
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
                                           const VkDeviceAddress resource_address,
                                           const size_t buffer_size,
                                           const uint32 descriptor_index = 0) {
      const auto binding_it = bindings_.find(binding);
      if (binding_it == bindings_.end()) {
        throw std::runtime_error("Invalid binding index.");
      }

      DescriptorBinding& descriptor_binding = binding_it->second;

      DescriptorUpdate update_info = {
        .type = type,
        .descriptorSize = map_descriptor_size(type),
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
      descriptor_binding.descriptor_size = map_descriptor_size(type);

      return *this;
    }

    DescriptorBufferInstance& write_acceleration_structure(const uint32_t binding, const VkDeviceAddress address, const uint32_t index = 0) {
      const auto binding_it = bindings_.find(binding);
      if (binding_it == bindings_.end()) {
        throw std::runtime_error("Invalid binding index.");
      }

      DescriptorBinding& descriptor_binding = binding_it->second;

      DescriptorUpdate update_info = {
        .type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        .descriptorSize = map_descriptor_size(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR),
        .binding = binding,
        .descriptorIndex = index,
        .tlas_address = address,
    };

      update_infos_.emplace_back(update_info);
      descriptor_binding.descriptor_size = map_descriptor_size(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);

      return *this;
    }

    DescriptorBufferInstance& write_image_array(
        const uint32 binding, const VkDescriptorType type,
        const std::vector<VkDescriptorImageInfo>& image_infos, const uint32 first_descriptor = 0) {
      const auto binding_it = bindings_.find(binding);
      if (binding_it == bindings_.end()) {
        throw std::runtime_error("Invalid binding index.");
      }

      DescriptorBinding& descriptor_binding = binding_it->second;

      const size_t descriptor_size = map_descriptor_size(type);
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