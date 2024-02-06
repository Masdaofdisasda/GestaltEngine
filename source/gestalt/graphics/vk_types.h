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

struct frame_buffer {
  AllocatedImage color_image;
  AllocatedImage depth_image;
};

struct double_buffered_frame_buffer {
  frame_buffer buffers[2];
  int write_buffer = 0;

  void switch_buffers() { write_buffer = (write_buffer + 1) % 2; }

  frame_buffer& get_write_buffer() { return buffers[write_buffer]; }

  frame_buffer& get_read_buffer() { return buffers[(write_buffer + 1) % 2]; }
};

struct per_frame_data {
  glm::mat4 view;
  glm::mat4 proj;
  glm::mat4 viewproj;
  glm::vec4 ambientColor;
  glm::vec4 sunlightDirection;  // w for sun power
  glm::vec4 sunlightColor;
};

//> mat_types
struct MaterialPipeline {
  VkPipeline pipeline;
  VkPipelineLayout layout;
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
  int material_id;
  int material_const_id;
  VkDeviceAddress vertexBuffer;
};
//< vbuf_types

struct engine_stats {
  float frametime;
  int triangle_count;
  int drawcall_count;
  float scene_update_time;
  float mesh_draw_time;
};

struct Bounds {
  glm::vec3 origin;
  glm::vec3 extents;
  float sphereRadius;
};

struct render_object {
  uint32_t index_count;
  uint32_t first_index;

  uint32_t material;
  Bounds bounds;
  glm::mat4 transform;
  VkDeviceAddress vertex_buffer_address;
};

struct draw_context {
  std::vector<render_object> opaque_surfaces;
  std::vector<render_object> transparent_surfaces;
};

struct render_config {
  bool always_opaque{false};
  bool enable_ssao{true};
  struct ssao_params {
    bool show_ssao_only = false;
    float scale = 0.75f;
    float bias = 0.008f;
    float zNear = 0.1f;
    float zFar = 1000.0f;
    float radius = 0.4f;
    float attScale = .95f;
    float distScale = 5.f;
  } ssao{};

  bool enable_hdr = true;
  struct hdr_params {
    bool show_bright_pass = false;
    float exposure{1.f};
    float maxWhite{1.f};
    float bloomStrength{1.f};
    float adaptationSpeed{1.f};
  } hdr{};

  bool enable_shadows{true};
  // todo : more settings
};

#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)