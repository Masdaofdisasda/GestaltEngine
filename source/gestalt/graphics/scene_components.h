﻿#pragma once
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
  std::vector<size_t> surfaces;
};

class CameraComponent {
public:
  CameraComponent(const glm::mat4& projectionMatrix);

  // Getters and setters for camera properties
  const glm::mat4& getProjectionMatrix() const;
  void setProjectionMatrix(const glm::mat4& projectionMatrix);

private:
  glm::mat4 projectionMatrix_;
};

enum class LightType { Directional, Point, Spot };

class LightComponent {
public:
  LightComponent(LightType type, const glm::vec3& color, float intensity);

  void setType(LightType type);
  void setColor(const glm::vec3& color);
  void setIntensity(float intensity);
  void setDirection(const glm::vec3& direction);
  void setSpotProperties(float innerCone, float outerCone);

  LightType getType() const;
  const glm::vec3& getColor() const;
  float getIntensity() const;
  const glm::vec3& getDirection() const;
  float getInnerCone() const;
  float getOuterCone() const;

private:
  LightType type_;
  glm::vec3 color_;
  float intensity_;
  glm::vec3 direction_;  // Used for directional and spot lights
  float innerCone_;      // Used for spot lights
  float outerCone_;      // Used for spot lights
};

struct pbr_config {
  glm::vec4 albedo_factor{0.f};
  bool use_albedo_tex{false};
  size_t albedo_tex{no_component};
  std::string albedo_uri;

  glm::vec2 metal_rough_factor{0.f};
  bool use_metal_rough_tex{false};
  size_t metal_rough_tex{no_component};
  std::string metal_rough_uri;

  float normal_scale = 1.f;
  bool use_normal_tex{false};
  size_t normal_tex{no_component};
  std::string normal_uri;

  glm::vec3 emissive_factor{0.f};
  bool use_emissive_tex{false};
  size_t emissive_tex{no_component};
  std::string emissive_uri;

  float occlusion_strength = 1.f;
  bool use_occlusion_tex{false};
  size_t occlusion_tex{no_component};
  std::string occlusion_uri;

  float alpha_cutoff = 0.5f;
  bool double_sided = false;
};

struct material_component {
  std::string name;
  MaterialInstance data;

  pbr_config config;
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

  bool is_valid() const { return entity != invalid_entity; }
  bool has_mesh() const { return mesh != no_component; }
};