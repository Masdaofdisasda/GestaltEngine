#pragma once

#include "vk_types.h"

namespace gestalt {
  namespace foundation {

    struct alignas(4) GpuVertexPosition {
      glm::vec3 position{0.f};
      float padding{0.f};
    };

    struct alignas(4) GpuVertexData {
      uint8_t normal[4];
      uint8_t tangent[4];
      uint16_t uv[2];
      float padding{0.f};
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
      std::array<AllocatedBuffer, 2> uniform_buffers;

      std::array<VkDescriptorSet, 2> descriptor_sets;
      VkDescriptorSetLayout descriptor_layout;
    };

    struct LightBuffers {
      AllocatedBuffer dir_light_buffer;
      AllocatedBuffer point_light_buffer;
      AllocatedBuffer view_proj_matrices;

      VkDescriptorSet descriptor_set = nullptr;
      VkDescriptorSetLayout descriptor_layout;
    };

    struct MeshBuffers {
      AllocatedBuffer index_buffer;
      AllocatedBuffer vertex_position_buffer;
      AllocatedBuffer vertex_data_buffer;

      VkDescriptorSet descriptor_set = nullptr;
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
  }  // namespace foundation
}  // namespace gestalt