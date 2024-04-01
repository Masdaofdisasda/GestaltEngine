#pragma once
#include <vector>
#include "vk_types.h"
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include "GpuResources.h"


namespace gestalt {
  namespace foundation {

    using entity = uint32_t;
    constexpr entity root_entity = 0;
    constexpr uint32_t invalid_entity = std::numeric_limits<uint32_t>::max();
    constexpr size_t no_component = std::numeric_limits<size_t>::max();
    constexpr size_t default_material = 0;

    struct alignas(16) Meshlet {
      glm::vec3 center;
      float radius;

      int8_t cone_axis[3];
      int8_t cone_cutoff;

      uint32_t data_offset;
      uint32_t mesh_index;
      uint8_t vertex_count;
      uint8_t triangle_count;
    };

    struct MeshSurface {
      std::vector<Meshlet> meshlets;

      uint32_t vertex_count;
      uint32_t index_count;
      uint32_t first_index;
      uint32_t vertex_offset;
      AABB local_bounds;

      size_t material = default_material;
    };

    struct MeshComponent {
      size_t mesh;
    };

    struct Mesh {
      std::string name;
      std::vector<MeshSurface> surfaces;
      AABB local_bounds;
    };

    struct CameraComponent {
      size_t camera_data;
      // TODO : add more camera settings
    };

    struct CameraData {
      glm::mat4 view_matrix{1.0f};
      glm::mat4 projection_matrix{1.0};
    };

    enum class LightType { kDirectional, kPoint, kSpot };

    struct LightComponent {
      LightType type;
      glm::vec3 color;
      float intensity;
      float inner_cone;  // Used for spot lights
      float outer_cone;  // Used for spot lights
      std::vector<size_t> light_view_projections;
      bool is_dirty = true;
    };

    constexpr auto unused_texture = std::numeric_limits<uint32_t>::max();

    struct PbrMaterial {
      bool use_albedo_tex{false};
      std::string albedo_uri;

      bool use_metal_rough_tex{false};
      std::string metal_rough_uri;

      bool use_normal_tex{false};
      std::string normal_uri;

      bool use_emissive_tex{false};
      std::string emissive_uri;

      bool use_occlusion_tex{false};
      std::string occlusion_uri;

      bool double_sided{false};
      bool transparent{false};

      struct MaterialConstants {
        uint32_t albedo_tex_index = unused_texture;
        uint32_t metal_rough_tex_index = unused_texture;
        uint32_t normal_tex_index = unused_texture;
        uint32_t emissive_tex_index = unused_texture;

        glm::vec4 albedo_factor{1.f};
        glm::vec2 metal_rough_factor = {.5f, 0.f};  // roughness, metallic
        float occlusionStrength{1.f};
        float alpha_cutoff{0.f};
        glm::vec3 emissiveFactor{0.f};
        uint32_t occlusion_tex_index = unused_texture;
        // float normalScale = 1.f;
      } constants;

      static_assert(sizeof(MaterialConstants) == 64);

      struct MaterialResources {
        TextureHandle albedo_image;
        VkSampler albedo_sampler;
        TextureHandle metal_rough_image;
        VkSampler metal_rough_sampler;
        TextureHandle normal_image;
        VkSampler normal_sampler;
        TextureHandle emissive_image;
        VkSampler emissive_sampler;
        TextureHandle occlusion_image;
        VkSampler occlusion_sampler;
      } resources;
    };

    struct Material {
      std::string name;
      PbrMaterial config;
      bool is_dirty = true;
    };

    struct TransformComponent {
      glm::vec3 position;
      glm::quat rotation;
      float scale;  // uniform scale for now

      size_t matrix = no_component;

      glm::vec3 parent_position;
      glm::quat parent_rotation;
      float parent_scale;

      mutable bool is_dirty = true;
    };

    struct NodeComponent {
      std::string name;
      entity parent = invalid_entity;
      std::vector<entity> children;
      AABB bounds;
      bool visible = true;
    };
  }  // namespace foundation
}  // namespace gestalt