#pragma once

#include <vk_types.h>

#include <filesystem>
#include <unordered_map>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include "materials.h"
#include "resource_manager.h"
#include "vk_descriptors.h"

// forward declaration
class render_engine;

using entity = uint32_t;
constexpr uint32_t invalid_entity = std::numeric_limits<uint32_t>::max();
constexpr size_t no_component = std::numeric_limits<size_t>::max();
constexpr size_t default_material = 0;
constexpr size_t identity_transform = 0;


struct surface {
  uint32_t vertex_count;
  uint32_t index_count;
  uint32_t first_index;
  uint32_t vertex_offset;
  Bounds bounds;

  size_t material = default_material;
};

struct mesh_component {
  std::vector <size_t> surfaces;
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

struct material_component {
  std::string name;
  MaterialInstance data;

  bool albedo_factor{false};
  bool albedo_tex{false};
  bool metal_rough_factor{false};
  bool metal_rough_tex{false};
  bool normal_tex{false};
  bool emissive_factor{false};
  bool emissive_tex{false};
  bool occlusion_tex{false};
};

struct transform_component {
  transform_component(const glm::vec3& position,
                      const glm::quat& rotation = glm::quat(0.f, 0.f, 0.f, 0.f),
                      const glm::vec3& scale = glm::vec3(1.f))
      :
    position(position), rotation(rotation), scale(scale) {}

  glm::mat4 getModelMatrix() const {
    glm::mat4 translationMatrix = translate(position);
    glm::mat4 rotationMatrix = mat4_cast(rotation);
    glm::mat4 scaleMatrix = glm::scale(scale);
    return translationMatrix * rotationMatrix * scaleMatrix;
  }

  glm::vec3 position;
  glm::quat rotation;
  glm::vec3 scale;
};

struct scene_object {
  std::string name;
  entity parent = invalid_entity;
  std::vector<entity> children;

  entity entity = invalid_entity; //Todo group
  size_t transform = identity_transform;

  size_t mesh = no_component;
  size_t camera = no_component;
  size_t light = no_component;

  bool is_valid() const { return entity != invalid_entity; }
  bool has_mesh() const { return mesh != no_component; }
};

// Component storage types
using entity_container = std::vector<entity>;
using mesh_container = std::vector<mesh_component>;
using camera_container = std::vector<CameraComponent>;
using light_container = std::vector<LightComponent>;
using material_container = std::vector<material_component>;
using transform_container = std::vector<transform_component>;

/**
 * @brief Class responsible for managing scenes, entities, and their components.
 */
class vk_scene_manager { //TODO split into data base and importer/loader
public:

  //TODO create container for per scene data in the future
  gpu_mesh_buffers mesh_buffers_;
  AllocatedBuffer material_data_buffer_;
  DescriptorAllocatorGrowable descriptorPool;

  void init(const vk_gpu& gpu, const resource_manager& resource_manager,
            const gltf_metallic_roughness& material);
  void cleanup();

  void load_scene_from_gltf(const std::string& file_path);
  void update_scene(draw_context& draw_context);
  void traverse_scene(entity entity, const glm::mat4& parent_transform, draw_context& draw_context);

  entity create_entity();
  size_t create_surface(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
  size_t create_mesh(std::vector<size_t> surfaces, const std::string& name);
  void add_mesh_component(entity entity, size_t mesh_index);
  void add_camera_component(entity entity, const CameraComponent& camera);
  void add_light_component(entity entity, const LightComponent& light);
  size_t create_material(MaterialPass pass_type,
                    const gltf_metallic_roughness::MaterialResources& resources,
                    material_component material = material_component{},
                    const std::string& name = "");
  void add_material_component(size_t surface, size_t material);
  void set_transform_component(entity entity, const glm::vec3& position,
                               const glm::quat& rotation = glm::quat(0.f, 0.f, 0.f, 0.f),
                               const glm::vec3& scale = glm::vec3(1.f));

  const camera_container& get_cameras() { return cameras_; }
  const mesh_container& get_meshes() { return meshes_; }
  const transform_container& get_transforms() { return transforms_; }
  const light_container& get_lights() { return lights_; }

  const std::vector<entity>& get_children(entity entity);
  std::optional<std::reference_wrapper<scene_object>> get_scene_object_by_entity(entity entity);


private:
  resource_manager resource_manager_;
  vk_deletion_service deletion_service_ = {};
  gltf_metallic_roughness gltf_material_ = {};

  vk_gpu gpu_ = {}; //TODO remove

  void init_default_data();

  std::vector<Vertex> vertices_;
  std::vector<uint32_t> indices_;
  std::vector<surface> surfaces_;

  // Component containers
  entity_container entities_;
  mesh_container meshes_;
  std::unordered_map<std::string, size_t> mesh_map_;
  camera_container cameras_;
  light_container lights_;
  material_container materials_;
  std::unordered_map<std::string, size_t> material_map_;
  transform_container transforms_{transform_component{glm::vec3(0)}};

  struct material {
    AllocatedImage color_image;
    AllocatedImage metallic_roughness_image;
    AllocatedImage normal_image;
    AllocatedImage emissive_image;
    AllocatedImage occlusion_image;

    AllocatedImage error_checkerboard_image;

    VkSampler default_sampler_linear;
    VkSampler default_sampler_nearest;
  } default_material_ = {};

  std::vector<AllocatedImage> images_;
  std::vector<VkSampler> samplers_;  // TODO

  std::unordered_map<entity, scene_object> scene_hierarchy_;
  scene_object root_ = {"root", invalid_entity, {}, invalid_entity};

  // Next available entity ID
  entity next_entity_id_ = 0;
};
