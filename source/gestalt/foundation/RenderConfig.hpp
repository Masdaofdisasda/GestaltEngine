#pragma once
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>


#include "common.hpp"

namespace gestalt::foundation {

    inline uint8 next_frame_index{1};
    inline uint8 current_frame_index{0};
    inline uint8 previous_frame_index{0};

    inline struct MeshletPushConstants {
      glm::mat4 viewProjection;

      float32 screenWidth, screenHeight, znear, zfar;  // symmetric projection parameters
      float32 frustum[4];  // data for left/right/top/bottom frustum planes

      float32 pyramidWidth, pyramidHeight;  // depth pyramid size in texels
      int32 clusterOcclusionEnabled;
    } meshlet_push_constants;

    struct RenderConfig {
      bool always_opaque{true};
      bool debug_aabb{true};

      struct SkyboxParams {
        glm::vec3 betaR = glm::vec3(5.22e-6, 9.19e-6, 33.1e-6);
        float32 pad1;
        glm::vec3 betaA = glm::vec3(0.000425, 0.001881, 0.000085);
        float32 pad2;
        glm::vec3 betaM = glm::vec3(5.61e-6, 2.1e-5, 2.1e-5);
        float pad3;
      } skybox{};

      bool enable_ssao{true};
      int ssao_quality{1};
      struct SsaoParams {
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
      struct HdrParams {
        float exposure{1.f};
        float maxWhite{1.35f};
        float bloomStrength{0.04f};
        float padding{1.f};
        glm::vec4 lift{0.f};
        glm::vec4 gamma{1.f};
        glm::vec4 gain{1.f};
        bool show_bright_pass = false;
        int toneMappingOption{2};
      } hdr{};

      struct LightAdaptationParams {
        float adaptation_speed_dark2light{.01f};
        float adaptation_speed_light2dark{.02f};
        float delta_time{0.16f};
        float min_luminance{0.0115f};
        float max_luminance{10.0f};
      } light_adaptation{};

      struct LightingParams {
        glm::mat4 invViewProj;
        int debug_mode{0};
        int num_dir_lights{0};
        int num_point_lights{0};
        int shading_mode{0};
        int shadow_mode{0};
        int ibl_mode{0};
      } lighting{};

      struct GridParams {
        float majorLineWidth = 0.065f;
        float minorLineWidth = 0.015f;
        float axisLineWidth = 0.080f;
        float axisDashScale = 1.33f;
        float majorGridDivision = 5.f;
      } grid{};

      static_assert(sizeof(LightingParams) <= 88);
    };
}  // namespace gestalt