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

enum class image_type { color, depth };

class AllocatedImage {
public:
  VkImage image = VK_NULL_HANDLE;
  VkImageView imageView = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
  VkExtent3D imageExtent = {};
  VkFormat imageFormat = VK_FORMAT_UNDEFINED;
  image_type type;
  VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  AllocatedImage(const image_type type = image_type::color) : type(type) {}

  VkExtent2D getExtent2D() const { return {imageExtent.width, imageExtent.height}; }
};

struct AllocatedBuffer {
  VkBuffer buffer;
  VmaAllocation allocation;
  VmaAllocationInfo info;
};

struct frame_buffer {

  frame_buffer() = default;

  void add_color_image(const AllocatedImage& image) {
    assert(image.type == image_type::color);
    if (color_image_.has_value()) {
      assert(color_image_.value().getExtent2D().height == image.getExtent2D().height);
      assert(color_image_.value().getExtent2D().width == image.getExtent2D().width);
    }

    color_image_ = image;
  }

  void add_depth_image(const AllocatedImage& image) {
    assert(image.type == image_type::depth);
    if (depth_image_.has_value()) {
      assert(depth_image_.value().getExtent2D().height == image.getExtent2D().height);
      assert(depth_image_.value().getExtent2D().width == image.getExtent2D().width);
    }

    depth_image_ = image;
  }

  AllocatedImage& get_color_image() {
    assert(color_image_.has_value());
    return color_image_.value();
  }

  AllocatedImage& get_depth_image() {
      assert(depth_image_.has_value());
      return depth_image_.value();
  }

private:
  std::optional<AllocatedImage> color_image_;
  std::optional<AllocatedImage> depth_image_;
};


struct double_buffered_frame_buffer {

  void switch_buffers() { write_buffer_index_ = (write_buffer_index_ + 1) % buffers_.size(); }

  frame_buffer& get_write_buffer() { return buffers_[write_buffer_index_]; }
  AllocatedImage& get_write_color_image() { return buffers_[write_buffer_index_].get_color_image(); }
  AllocatedImage& get_write_depth_image() { return buffers_[write_buffer_index_].get_depth_image(); }

  frame_buffer& get_read_buffer() { return buffers_[(write_buffer_index_ + 1) % buffers_.size()]; }
  AllocatedImage& get_read_color_image() {
    return buffers_[(write_buffer_index_ + 1) % buffers_.size()].get_color_image();
  }
  AllocatedImage& get_read_depth_image() {
    return buffers_[(write_buffer_index_ + 1) % buffers_.size()].get_depth_image();
  }

private:
  std::array<frame_buffer, 2> buffers_;
  size_t write_buffer_index_ = 0;
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
  int ssao_quality{1};
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

  bool enable_hdr{true};
  int bloom_quality{3};
  struct hdr_params {
    float exposure{1.f};
    float maxWhite{1.35f};
    float bloomStrength{0.1f};
    float adaptationSpeed{1.f};
    glm::vec4 lift{0.f};
    glm::vec4 gamma{1.f};
    glm::vec4 gain{1.f};
    bool show_bright_pass = false;
    int toneMappingOption{2};
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