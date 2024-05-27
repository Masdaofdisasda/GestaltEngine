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

#include "common.hpp"

namespace gestalt::foundation {

    struct alignas(128) PerFrameData {
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
      float32 frametime;
      int32 triangle_count;
      int32 drawcall_count;
      float32 scene_update_time;
      float32 mesh_draw_time;
    };

    struct Bounds {
      glm::vec3 origin;
      glm::vec3 extents;
      float32 sphereRadius;
    };

    struct AABB {
      glm::vec3 min{glm::vec3(std::numeric_limits<float32>::max())};
      glm::vec3 max{glm::vec3(std::numeric_limits<float32>::lowest())};

      mutable bool is_dirty = true;
    };

    struct RenderObject {
      uint32 index_count;
      uint32 first_index;
      uint32 vertex_offset;

      uint32 material;
      glm::mat4 transform;
    };

    struct DrawContext {
      std::vector<RenderObject> opaque_surfaces;
      std::vector<RenderObject> transparent_surfaces;
    };

}  // namespace gestalt foundation

#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)

