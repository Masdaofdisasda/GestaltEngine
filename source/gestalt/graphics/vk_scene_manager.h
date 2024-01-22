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

struct mesh_component {
  uint32_t vertex_count;
  uint32_t index_count;
  uint32_t first_index;
  uint32_t vertex_offset;
  Bounds bounds;
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
};

struct transform_component {
  transform_component(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale) :
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
  std::vector<entity> children;

  entity entity = invalid_entity;
  size_t mesh = no_component;
  size_t camera = no_component;
  size_t light = no_component;
  size_t material = no_component;
  size_t transform = no_component;
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
class vk_scene_manager {
public:

  gpu_mesh_buffers mesh_buffers_;
  DescriptorAllocatorGrowable descriptorPool;

  void init(const vk_gpu& gpu, const resource_manager& resource_manager, gltf_metallic_roughness& material) {
    gpu_ = gpu;
    resource_manager_ = resource_manager;
    deletion_service_.init(gpu.device, gpu.allocator);
    gltf_material_ = material;

    init_default_data();
    load_scene_from_gltf("");
  }

  void cleanup() {
       deletion_service_.flush();
  }

  void load_scene_from_gltf(const std::string& filename);

  entity create_entity() {
    const entity new_entity = {next_entity_id_++};
    entities_.push_back(new_entity);

    const scene_object object = {.entity = new_entity};
    entity_hierarchy_[new_entity] = object;

    return new_entity;
  }

  void add_mesh_component(const entity entity, std::vector<Vertex>& vertices,
                          std::vector<uint32_t>& indices);

  void add_camera_component(entity entity, const CameraComponent& camera) {
    cameras_.push_back(camera);
    scene_object& object = entity_hierarchy_[entity];
    object.camera = cameras_.size() - 1;
  }

  void add_light_component(entity entity, const LightComponent& light) {
    lights_.push_back(light);
    scene_object& object = entity_hierarchy_[entity];
    object.camera = lights_.size() - 1;
  }

  void add_material(MaterialPass pass_type,
                    const gltf_metallic_roughness::MaterialResources& resources,
                    const std::string& name = "") {

    materials_.emplace_back(material_component{
        .name = name,
        .data = gltf_material_.write_material(gpu_.device, pass_type, resources, descriptorPool)});
    std::string key = name.empty() ? "material_" + std::to_string(materials_.size()) : name;
    material_map_[key] = materials_.size() - 1;
  }

  void add_material_component(const entity entity, const std::string& name) {
    scene_object& object = entity_hierarchy_[entity];
    size_t material_index = material_map_[name];
    object.material = material_index;
  }

  void add_transform_component(entity entity, const glm::vec3& position, const glm::quat& rotation,
                               const glm::vec3& scale = glm::vec3(1.f));

  const camera_container& get_cameras() const;

  const mesh_container& get_meshes() { return meshes_; }

  const transform_container& get_transforms() { return transforms_; }

  const light_container& get_lights() const;


  const std::vector<entity>& get_children(entity entity) const;


  void update_scene(draw_context& draw_context);

  // Add more methods for other operations like updating, rendering, etc.

private:
  resource_manager resource_manager_;
  vk_deletion_service deletion_service_;
  gltf_metallic_roughness gltf_material_;

  vk_gpu gpu_; //TODO remove

  void init_default_data();

  std::vector<Vertex> vertices_;
  std::vector<uint32_t> indices_;

  // Component containers
  entity_container entities_;
  mesh_container meshes_;
  camera_container cameras_;
  light_container lights_;
  material_container materials_;
  std::unordered_map<std::string, size_t> material_map_;
  transform_container transforms_;

  // Add similar mappings for other components

  std::unordered_map<entity, scene_object> entity_hierarchy_;

  scene_object root_ = {"root", {}};

  std::string default_material_name_ = "default_material";

  // Next available entity ID
  entity next_entity_id_ = 0;
};
