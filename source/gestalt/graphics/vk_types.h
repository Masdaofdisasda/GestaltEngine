// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once

#define SDL_MAIN_HANDLED

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

struct GPUGLTFMaterial {
  glm::vec4 colorFactors;
  glm::vec4 metal_rough_factors;
  glm::vec4 extra[14];
};

static_assert(sizeof(GPUGLTFMaterial) == 256);

struct gpu_scene_data {
  glm::mat4 view;
  glm::mat4 proj;
  glm::mat4 viewproj;
  glm::vec4 ambientColor;
  glm::vec4 sunlightDirection;  // w for sun power
  glm::vec4 sunlightColor;
};

//> mat_types
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
//< mat_types
//> vbuf_types
struct Vertex {
  glm::vec3 position;
  float uv_x;
  glm::vec3 normal;
  float uv_y;
  glm::vec4 color;
};

// holds the resources needed for a mesh
struct gpu_mesh_buffers {
  AllocatedBuffer indexBuffer;
  AllocatedBuffer vertexBuffer;
  VkDeviceAddress vertexBufferAddress;
};

// push constants for our mesh object draws
struct GPUDrawPushConstants {
  glm::mat4 worldMatrix;
  VkDeviceAddress vertexBuffer;
};
//< vbuf_types

//> node_types
struct draw_context;

// base class for a renderable dynamic object
class IRenderable {
  virtual void Draw(const glm::mat4& topMatrix, draw_context& ctx) = 0;
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

  virtual void Draw(const glm::mat4& topMatrix, draw_context& ctx) {
    // draw children
    for (auto& c : children) {
      c->Draw(topMatrix, ctx);
    }
  }
};

struct engine_stats {
  float frametime;
  int triangle_count;
  int drawcall_count;
  float scene_update_time;
  float mesh_draw_time;
};


struct default_material {
  AllocatedImage white_image;
  AllocatedImage black_image;
  AllocatedImage grey_image;
  AllocatedImage error_checkerboard_image;

  VkSampler default_sampler_linear;
  VkSampler default_sampler_nearest;
};

struct Bounds {
  glm::vec3 origin;
  float sphereRadius;
  glm::vec3 extents;
};

struct GLTFMaterial {
  MaterialInstance data;
};

struct GeoSurface {
  uint32_t startIndex;
  uint32_t count;
  Bounds bounds;
  std::shared_ptr<GLTFMaterial> material;
};

struct MeshAsset {
  std::string name;

  std::vector<GeoSurface> surfaces;
  gpu_mesh_buffers meshBuffers;
};
struct mesh_node : Node {
  std::shared_ptr<MeshAsset> mesh;

  void Draw(const glm::mat4& topMatrix, draw_context& ctx) override;
};

struct render_object {
  uint32_t index_count;
  uint32_t first_index;
  VkBuffer index_buffer;

  MaterialInstance* material;
  Bounds bounds;
  glm::mat4 transform;
  VkDeviceAddress vertex_buffer_address;
};

struct draw_context {
  std::vector<render_object> opaque_surfaces;
  std::vector<render_object> transparent_surfaces;
};

#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)