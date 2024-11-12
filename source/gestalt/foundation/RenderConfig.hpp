#pragma once
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>


#include "common.hpp"

namespace gestalt::foundation {

    inline struct MeshletPushConstants {
      int cullFlags{1};
      float32 pyramidWidth, pyramidHeight;  // depth pyramid size in texels
      int32 clusterOcclusionEnabled;
    } meshlet_push_constants;

    struct RenderConfig {
      bool always_opaque{true};
      bool debug_aabb_bvh{false};
      bool debug_bounds_mesh{false};

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
      float32 ssao_radius{5.f};
      float32 depthThreshold{0.1f};
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
        float bloomStrength{0.01f};
        float padding{1.f};
        glm::vec4 lift{0.f};
        glm::vec4 gamma{1.f};
        glm::vec4 gain{1.f};
        bool show_bright_pass = false;
        int toneMappingOption{4};
      } hdr{};

      struct LightAdaptationParams {
        float adaptation_speed_dark2light{1.f};
        float adaptation_speed_light2dark{10.f};
        float delta_time{0.16f};
        float min_luminance{0.001f};
        float max_luminance{10000.f};
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

      struct VolumetricLightingParams {
        glm::vec2 halton_xy{0.f, 0.f};  // no jitter
        float32 temporal_reprojection_jitter_scale{0.f};
        float32 density_modifier{1.f};

        float32 noise_scale{0.1f};
        uint32 noise_type{0};

        float32 volumetric_noise_position_multiplier{1.f};
        float32 volumetric_noise_speed_multiplier{1.f};
        float32 height_fog_density{0.3f};
        float32 height_fog_falloff{0.7f};

        glm::vec3 box_position{0.f};
        float32 box_fog_density{0.f};

        int32 enable_spatial_filter{1};

        glm::vec3 box_size{10.f};
        float32 scattering_factor{0.05f};
        float32 phase_anisotropy{2.f};
        uint32 phase_type{3};
      } volumetric_lighting;


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