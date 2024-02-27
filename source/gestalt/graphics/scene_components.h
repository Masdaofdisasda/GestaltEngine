#pragma once
#include <vector>
#include "vk_types.h"
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>


using entity = uint32_t;
constexpr uint32_t invalid_entity = std::numeric_limits<uint32_t>::max();
constexpr size_t no_component = std::numeric_limits<size_t>::max();
constexpr size_t default_material = 0;

struct mesh_surface {
  uint32_t vertex_count;
  uint32_t index_count;
  uint32_t first_index;
  uint32_t vertex_offset;
  Bounds bounds;

  size_t material = default_material;
};

struct mesh_component {
  std::string name;
  std::vector<size_t> surfaces;
};

struct camera_component {
  glm::mat4 view_matrix{1.0f};
  glm::mat4 projection_matrix{1.0};
};

enum class light_type { directional, point, spot };

struct light_component {
  light_component(const glm::vec3& color, const float intensity, const glm::vec3 direction)
      : type(light_type::directional), color(color), intensity(intensity), direction(direction) {}
  light_component(const glm::vec3& color, const float intensity)
      : type(light_type::point), color(color), intensity(intensity) {}
  light_component(const glm::vec3& color, const float intensity, const glm::vec3 direction,
                  const float inner_cone, const float outer_cone)
      : type(light_type::spot),
        color(color),
        intensity(intensity),
        direction(direction),
        inner_cone(inner_cone),
        outer_cone(outer_cone) {}

  light_type type = light_type::point;
  glm::vec3 color = glm::vec3(1.f);
  float intensity = 1.0f;
  glm::vec3 direction = glm::vec3(0);  // Used for directional and spot lights
  float inner_cone = 0.f;              // Used for spot lights
  float outer_cone = 0.f;              // Used for spot lights
};

constexpr auto unused_texture = std::numeric_limits<uint32_t>::max();

struct pbr_material {
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

  struct material_constants {
    uint32_t albedo_tex_index = unused_texture;
    uint32_t metal_rough_tex_index = unused_texture;
    uint32_t normal_tex_index = unused_texture;
    uint32_t emissive_tex_index = unused_texture;

    glm::vec4 albedo_factor{1.f};
    glm::vec2 metal_rough_factor = {.5f, 0.f}; // roughness, metallic
    float occlusionStrength{1.f};
    float alpha_cutoff{0.f};
    glm::vec3 emissiveFactor{1.f};
    uint32_t occlusion_tex_index = unused_texture;
    // float normalScale = 1.f;
  } constants;

  static_assert(sizeof(material_constants) == 64);

  struct material_resources {
    AllocatedImage albedo_image;
    VkSampler albedo_sampler;
    AllocatedImage metal_rough_image;
    VkSampler metal_rough_sampler;
    AllocatedImage normal_image;
    VkSampler normal_sampler;
    AllocatedImage emissive_image;
    VkSampler emissive_sampler;
    AllocatedImage occlusion_image;
    VkSampler occlusion_sampler;
  } resources;
};

struct material_component {
  std::string name;
  pbr_material config;
};

struct transform_component {
  explicit transform_component(const glm::vec3& position,
                               const glm::quat& rotation = glm::quat(0.f, 0.f, 0.f, 0.f),
                               const glm::vec3& scale = glm::vec3(1.f))
      : position(position), rotation(rotation), scale(scale) {}

  glm::mat4 get_model_matrix() const {
    return translate(position) * mat4_cast(rotation) * glm::scale(scale);
  }

  glm::vec3 position;
  glm::quat rotation;
  glm::vec3 scale;
  mutable bool is_dirty = true;
};

struct entity_component {
  std::string name;
  entity parent = invalid_entity;
  std::vector<entity> children;

  entity entity = invalid_entity;
  size_t transform = no_component;

  size_t mesh = no_component;
  size_t camera = no_component;
  size_t light = no_component;

  bool visible{true};

  bool is_valid() const { return entity != invalid_entity; }
  bool has_mesh() const { return mesh != no_component; }
};