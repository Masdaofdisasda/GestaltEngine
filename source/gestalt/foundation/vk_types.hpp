#pragma once

#define SDL_MAIN_HANDLED

#include "common.hpp"

#include <vector>

#define VK_NO_PROTOTYPES
#include <volk.h>
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>


namespace gestalt::foundation {

    struct alignas(16) PerFrameData {
      glm::mat4 view{1.f};
      glm::mat4 inv_view{1.f};
      glm::mat4 proj{1.f};
      glm::mat4 inv_viewProj{1.f};
      glm::mat4 cullView{1.f};
      glm::mat4 cullProj{1.f};
      float P00, P11, znear, zfar;  // symmetric projection parameters
      glm::vec4 frustum[6];
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

    struct BoundingSphere {
      glm::vec3 center;
      float radius;
      mutable bool is_dirty = true;
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

