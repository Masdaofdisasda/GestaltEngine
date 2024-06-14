#pragma once

#include "vk_types.hpp"

#include <vma/vk_mem_alloc.h>

#include <EngineConfig.hpp>
#include <array>

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

    struct MeshDraw {
      // TRS
      glm::vec3 position;
      float32 scale;
      glm::vec4 orientation;

      // Bounding box
      // Meshlet data
      glm::vec3 min;
      uint32 meshlet_offset;
      glm::vec3 max;
      uint32 meshlet_count;

      // Vertex data
      uint32 vertex_count;
      uint32 index_count;
      uint32 first_index;
      uint32 vertex_offset;

      // Material
      uint32 materialIndex;
      uint32 pad[3];
    };

  static_assert(sizeof(MeshDraw) == 96);

    struct MeshTaskCommand {
      uint32 meshDrawId;
      uint32 taskOffset;
      uint32 taskCount;
      uint32 pad;
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
      void setLayout(VkImageLayout layout) {
        currentLayout = layout;
      }
      VkImageLayout getLayout() const {
        return currentLayout;
      }
      void setFormat(VkFormat format) { imageFormat = format; }
      VkFormat getFormat() const { return imageFormat; }
      TextureType getType() const { return type; }
    };

    struct AllocatedBuffer {
      VkBuffer buffer;
      VmaAllocation allocation;
      VmaAllocationInfo info;
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

      struct MaterialData {
      VkDescriptorSet resource_set;
      VkDescriptorSetLayout resource_layout;

      AllocatedBuffer constants_buffer;
      VkDescriptorSet constants_set;
      VkDescriptorSetLayout constants_layout;
    };

    // BUFFER TYPES

    struct PerFrameDataBuffers{
      PerFrameData data;
      std::array<AllocatedBuffer, getFramesInFlight()> uniform_buffers;

      std::array<VkDescriptorSet, getFramesInFlight()> descriptor_sets;
      VkDescriptorSetLayout descriptor_layout;
    };

    struct LightBuffers { //TODO double buffering
      AllocatedBuffer dir_light_buffer;
      AllocatedBuffer point_light_buffer;
      AllocatedBuffer view_proj_matrices;

      VkDescriptorSet descriptor_set = nullptr;
      VkDescriptorSetLayout descriptor_layout;
    };

    struct MeshBuffers {
      AllocatedBuffer index_buffer; // regular index buffer
      AllocatedBuffer vertex_position_buffer; // only vertex positions
      AllocatedBuffer vertex_data_buffer; // normals, tangents, uvs

      AllocatedBuffer meshlet_buffer; // meshlets
      AllocatedBuffer meshlet_vertices;
      AllocatedBuffer meshlet_triangles; // meshlet indices
      std::array<AllocatedBuffer, getFramesInFlight()> meshlet_task_commands_buffer;
      std::array<AllocatedBuffer, getFramesInFlight()> mesh_draw_buffer;   // TRS, material id
      std::array<AllocatedBuffer, getFramesInFlight()> draw_count_buffer;  // number of draws

      std::array<VkDescriptorSet, getFramesInFlight()> descriptor_sets;
      VkDescriptorSetLayout descriptor_layout;
    };

    struct IblBuffers {
      TextureHandle environment_map;
      TextureHandle environment_irradiance_map;
      TextureHandle bdrf_lut;

      VkSampler cube_map_sampler;

      VkDescriptorSet descriptor_set = nullptr;
      VkDescriptorSetLayout descriptor_layout;
    };

}  // namespace gestalt