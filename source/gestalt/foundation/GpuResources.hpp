#pragma once

#include "vk_types.hpp"

#include <vma/vk_mem_alloc.h>
#include <glm/gtc/quaternion.hpp>

#include <EngineConfig.hpp>
#include <array>

namespace gestalt::foundation {
  class IGpu;
}

namespace gestalt::foundation {

  struct alignas(4) GpuVertexPosition {
    glm::vec3 position{0.f};
    float32 padding{0.f};
  };

  struct alignas(4) GpuVertexData {
    uint8 normal[4];
    uint8 tangent[4];
    uint16 uv[2];
    float32 padding{0.f};
  };

  /**
   * \brief A MeshDraw contains all the data needed to draw a single MeshSurface. It has
   * transformation data for world space transformation, bounding data for culling, and references
   * to the vertex and index data for the MeshSurface.
   */
  struct MeshDraw {
    // TRS
    glm::vec3 position;
    float32 scale;
    glm::quat orientation;

    // Bounding sphere
    // Meshlet data
    glm::vec3 center;
    float32 radius;
    uint32 meshlet_offset;
    uint32 meshlet_count;

    // Vertex data , maybe unneeded?
    uint32 vertex_count;
    uint32 index_count;
    uint32 first_index;
    uint32 vertex_offset;

    // Material
    uint32 materialIndex;
    int32 pad;
  };

  static_assert(sizeof(MeshDraw) % 16 == 0);

  struct alignas(16) MeshTaskCommand {
    uint32 meshDrawId;
    uint32 taskOffset;
    uint32 taskCount;
  };

  enum class TextureType { kColor, kDepth, kCubeMap };

  class TextureHandle {
    VkFormat imageFormat = VK_FORMAT_UNDEFINED;
    TextureType type;
    VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  public:
    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkExtent3D imageExtent = {};

    TextureHandle(const TextureType type = TextureType::kColor) : type(type) {}

    VkExtent2D getExtent2D() const { return {imageExtent.width, imageExtent.height}; }
    void setLayout(VkImageLayout layout) { currentLayout = layout; }
    VkImageLayout getLayout() const { return currentLayout; }
    void setFormat(VkFormat format) { imageFormat = format; }
    VkFormat getFormat() const { return imageFormat; }
    TextureType getType() const { return type; }
  };

  struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
    VkDeviceAddress address;

    VkAccessFlags2 currentAccess = 0;        // Current access flags
    VkPipelineStageFlags2 currentStage = 0;  // Current pipeline stage
    VkBufferUsageFlags usage;                // Buffer usage flags
    VmaMemoryUsage memory_usage;
  };

  
  struct DescriptorUpdate {
    VkDescriptorType type;
    size_t descriptorSize;
    uint32 binding;
    uint32 descriptorIndex;
    VkDescriptorAddressInfoEXT addr_info; // only relevant for buffer resources
    VkDescriptorImageInfo image_info; // only relevant for image resources
  };

  struct DescriptorBinding {
    size_t descriptorSize; // size of a single descriptor in bytes
    uint32 descriptorCount{1}; // NOTE: descriptorCount is only relevant for image arrays
    uint32 binding; // binding index in the descriptor set
    VkDeviceSize offset; // offset into the descriptor buffer where this binding starts
  };

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

  struct alignas(32) GpuDirectionalLight {
    glm::vec3 color;
    float intensity;
    glm::vec3 direction;
    uint32_t viewProj;
  };

  struct alignas(32) GpuPointLight {
    glm::vec3 color;
    float intensity;
    glm::vec3 position;
    bool enabled;
  };

  // BUFFER TYPES
  struct BufferCollection {
    virtual ~BufferCollection() = default;
    virtual std::vector<std::shared_ptr<AllocatedBuffer>> get_buffers(int16 frame_index) const = 0;
    virtual std::shared_ptr<DescriptorBuffer> get_descriptor_buffer(
        int16 frame_index) const
        = 0;
  };

  struct MaterialBuffers final : BufferCollection, NonCopyable<MaterialBuffers> {
    std::shared_ptr<AllocatedBuffer> uniform_buffer;

    TextureHandle environment_map;
    TextureHandle environment_irradiance_map;
    TextureHandle bdrf_lut;

    VkSampler cube_map_sampler;

    std::shared_ptr<DescriptorBuffer> descriptor_buffer;

    std::vector<std::shared_ptr<AllocatedBuffer>> get_buffers(int16 frame_index) const override {
      return {uniform_buffer};
    }

    std::shared_ptr<DescriptorBuffer> get_descriptor_buffer(
        int16 frame_index) const override {
      return descriptor_buffer;
    }
  };

  struct PerFrameDataBuffers final : BufferCollection {
    PerFrameData data;
    bool freezeCullCamera = false;
    std::array<std::shared_ptr<AllocatedBuffer>, getFramesInFlight()> uniform_buffers;

    std::array<std::shared_ptr<DescriptorBuffer>, getFramesInFlight()> descriptor_buffers;

    std::vector<std::shared_ptr<AllocatedBuffer>> get_buffers(int16 frame_index) const override {
      return {uniform_buffers[frame_index]};
    }

    std::shared_ptr<DescriptorBuffer> get_descriptor_buffer(
        int16 frame_index) const override {
      return descriptor_buffers[frame_index];
    }
  };

  struct LightBuffers final : BufferCollection {
    std::shared_ptr<AllocatedBuffer> dir_light_buffer;
    std::shared_ptr<AllocatedBuffer> point_light_buffer;
    std::shared_ptr<AllocatedBuffer> view_proj_matrices;

    std::shared_ptr<DescriptorBuffer> descriptor_buffer;

    std::vector<std::shared_ptr<AllocatedBuffer>> get_buffers(int16 frame_index) const override {
      return {dir_light_buffer, point_light_buffer, view_proj_matrices};
    }

    std::shared_ptr<DescriptorBuffer> get_descriptor_buffer(
        int16 frame_index) const override {
      return descriptor_buffer;
    }
  };

  struct MeshBuffers final : BufferCollection {
    std::shared_ptr<AllocatedBuffer> index_buffer;            // regular index buffer
    std::shared_ptr<AllocatedBuffer> vertex_position_buffer;  // only vertex positions
    std::shared_ptr<AllocatedBuffer> vertex_data_buffer;      // normals, tangents, uvs

    std::shared_ptr<AllocatedBuffer> meshlet_buffer;  // meshlets
    std::shared_ptr<AllocatedBuffer> meshlet_vertices;
    std::shared_ptr<AllocatedBuffer> meshlet_triangles;  // meshlet indices
    std::array<std::shared_ptr<AllocatedBuffer>, getFramesInFlight()> meshlet_task_commands_buffer;
    std::array<std::shared_ptr<AllocatedBuffer>, getFramesInFlight()>
        mesh_draw_buffer;  // TRS, material id
    std::array<std::shared_ptr<AllocatedBuffer>, getFramesInFlight()>
        draw_count_buffer;  // number of draws

    std::array<std::shared_ptr<DescriptorBuffer>, getFramesInFlight()> descriptor_buffers;

    std::vector<std::shared_ptr<AllocatedBuffer>> get_buffers(int16 frame_index) const override {
      return {index_buffer,
              vertex_position_buffer,
              vertex_data_buffer,
              meshlet_buffer,
              meshlet_vertices,
              meshlet_triangles,
              meshlet_task_commands_buffer[frame_index],
              mesh_draw_buffer[frame_index],
              draw_count_buffer[frame_index]};
    }

    std::shared_ptr<DescriptorBuffer> get_descriptor_buffer(
        int16 frame_index) const override {
      return descriptor_buffers[frame_index];
    }
  };

}  // namespace gestalt::foundation