// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vma/vk_mem_alloc.h>

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

struct AllocatedImage {
  VkImage image;
  VkImageView imageView;
  VmaAllocation allocation;
  VkExtent3D imageExtent;
  VkFormat imageFormat;
};

struct AllocatedBuffer {
  VkBuffer buffer;
  VmaAllocation allocation;
  VmaAllocationInfo info;
};

struct Vertex {
  glm::vec3 position;
  float uv_x;
  glm::vec3 normal;
  float uv_y;
  glm::vec4 color;
};

static_assert(sizeof(Vertex) == 48);

enum class MaterialPass : uint8_t { MainColor, Transparent, Other };

struct MaterialPipeline {
  VkPipeline pipeline;
  VkPipelineLayout layout;
};

struct MaterialInstance {
  MaterialPipeline* pipeline;
  VkDescriptorSet materialSet;
  MaterialPass passType;
};

struct DrawContext;

// base class for a renderable dynamic object
class IRenderable {
  virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
};

// implementation of a drawable scene node.
// the scene node can hold children and will also keep a transform to propagate
// to them
struct Node : public IRenderable {
  // parent pointer must be a weak pointer to avoid circular dependencies
  std::weak_ptr<Node> parent;
  std::vector<std::shared_ptr<Node>> children;

  glm::mat4 localTransform;
  glm::mat4 worldTransform;

  void refreshTransform(const glm::mat4& parentMatrix) {
    worldTransform = parentMatrix * localTransform;
    for (auto c : children) {
      c->refreshTransform(worldTransform);
    }
  }

  virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) {
    // draw children
    for (auto& c : children) {
      c->Draw(topMatrix, ctx);
    }
  }
};

// holds the resources needed for a mesh
struct GPUMeshBuffers {
  AllocatedBuffer indexBuffer;
  AllocatedBuffer vertexBuffer;
  VkDeviceAddress vertexBufferAddress;
};

// push constants for our mesh object draws
struct GPUDrawPushConstants {
  glm::mat4 worldMatrix;
  VkDeviceAddress vertexBuffer;
};

#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)