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
        glm::vec3 betaR = glm::vec3(5.5e-6f, 13.0e-6f, 22.4e-6f);
        uint32 showEnviromentMap{0};
        glm::vec3 betaA = glm::vec3(0.00065f, 0.0015f, 0.0001f);
        float32 pad2;
        glm::vec3 betaM = glm::vec3(2.0e-5f, 2.0e-5f, 1.0e-5f);
        float32 pad3;
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
        glm::vec4 lift{0.f, 0.f, 0.f, 0.f};
        glm::vec4 gamma{1.0f, 1.f, 1.f, 1.f};
        glm::vec4 gain{1.f, 1.f, 1.f, 1.f};
        bool show_bright_pass = false;
        int toneMappingOption{4};
        float vignette_radius{2.f};
        float vignette_softness{0.f};

        float iso = 100.0f;                 // e.g., [50..6400]
        float shutter = 1.f;       // in seconds (like 1/60s)
        float aperture = 2.0f;              // f-stop
        float exposureCompensation = 0.0f;  // EV comp

        float saturation{1.0f};
        float contrast{1.1f};
      } hdr{};

      struct LuminanceParams {
        float min_log_lum{-10.f};        // minimum log2 luminance
        float max_log_lum{10.f};                          // maximum log2 luminance
        float log_lum_range{max_log_lum - min_log_lum};  // log2 luminance range
        float inv_log_lum_range{1.0f / log_lum_range};    // 1 / log2 luminance range

        float time_coeff{0.1f};
        float num_pixels{1300 * 900};
        float width{1300};
        float height{900};
      } luminance_params;

      struct alignas(32) LightingParams {
        glm::mat4 invViewProj;

        int debug_mode{0};
        int num_dir_lights{0};
        int num_point_lights{0};
        int num_spot_lights{0};

        int shading_mode{0};
        int shadow_mode{0};
        int ibl_mode{0};
        int pad{0};
      } lighting{};

      struct VolumetricLightingParams {
        glm::vec2 halton_xy{0.f, 0.f};  // no jitter
        float32 temporal_reprojection_jitter_scale{0.f};
        float32 density_modifier{0.07f};

        float32 noise_scale{0.1f};
        uint32 noise_type{0};

        float32 volumetric_noise_position_multiplier{1.f};
        float32 volumetric_noise_speed_multiplier{1.f};
        float32 height_fog_density{3.5f};
        float32 height_fog_falloff{0.4f};

        glm::vec3 box_position{-1200.f, 0.f, -600.f};
        float32 box_fog_density{0.3f};

        int32 enable_spatial_filter{1};

        glm::vec3 box_size{150.f, 500.f, 200.f};
        float32 scattering_factor{0.01f};
        float32 phase_anisotropy{.05f};
        uint32 phase_type{3};
      } volumetric_lighting;


      struct GridParams {
        float majorLineWidth = 0.065f;
        float minorLineWidth = 0.015f;
        float axisLineWidth = 0.080f;
        float axisDashScale = 1.33f;
        float majorGridDivision = 5.f;
      } grid{};

      static_assert(sizeof(LightingParams) == 32*3);
    };
}  // namespace gestalt