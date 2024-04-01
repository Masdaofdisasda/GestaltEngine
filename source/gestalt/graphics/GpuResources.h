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

    enum class TextureType { kColor, kDepth };

    class TextureHandle {
    public:
      VkImage image = VK_NULL_HANDLE;
      VkImageView imageView = VK_NULL_HANDLE;
      VmaAllocation allocation = VK_NULL_HANDLE;
      VkExtent3D imageExtent = {};
      VkFormat imageFormat = VK_FORMAT_UNDEFINED;
      TextureType type;
      VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

      TextureHandle(const TextureType type = TextureType::kColor) : type(type) {}

      VkExtent2D getExtent2D() const { return {imageExtent.width, imageExtent.height}; }
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

  }  // namespace foundation
}  // namespace gestalt