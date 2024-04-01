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

namespace gestalt {
  namespace foundation {

    struct PerFrameData {
      glm::mat4 view{1.f};
      glm::mat4 proj{1.f};
      glm::mat4 viewproj{1.f};
      glm::mat4 inv_viewproj{1.f};
    };

    struct MaterialPipeline {
      VkPipeline pipeline;
      VkPipelineLayout layout;
    };

    struct EngineStats {
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

    struct AABB {
      glm::vec3 min{glm::vec3(std::numeric_limits<float>::max())};
      glm::vec3 max{glm::vec3(std::numeric_limits<float>::lowest())};

      mutable bool is_dirty = true;
    };

    struct RenderObject {
      uint32_t index_count;
      uint32_t first_index;
      uint32_t vertex_offset;

      uint32_t material;
      glm::mat4 transform;
    };

    struct DrawContext {
      std::vector<RenderObject> opaque_surfaces;
      std::vector<RenderObject> transparent_surfaces;
    };

  }  // namespace foundation
}  // namespace gestalt

#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)

