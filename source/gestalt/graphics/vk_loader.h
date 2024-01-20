#pragma once

#include <vk_types.h>

#include <filesystem>
#include <unordered_map>
#include <fastgltf/types.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include "resource_manager.h"
#include "vk_descriptors.h"

// forward declaration
class render_engine;

struct LoadedGLTF : IRenderable {
  // storage for all the data on a given glTF file
  std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
  std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
  std::unordered_map<std::string, AllocatedImage> images;
  std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

  // nodes that dont have a parent, for iterating through the file in tree order
  std::vector<std::shared_ptr<Node>> topNodes;

  std::vector<VkSampler> samplers;

  DescriptorAllocatorGrowable descriptorPool;

  AllocatedBuffer materialDataBuffer;

  render_engine* creator;

  ~LoadedGLTF() { clearAll(); };

  virtual void Draw(const glm::mat4& topMatrix, draw_context& ctx);

private:
  void clearAll();
};

std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(render_engine* engine,
                                                    std::string_view filePath);

std::optional<AllocatedImage> load_image(render_engine* engine, fastgltf::Asset& asset,
                                         fastgltf::Image& image);

using entity = uint32_t;

struct entity_data {
  entity parent;
  std::vector<entity> children;
};


struct mesh_component {
    uint32_t vertex_count_;
   uint32_t index_count_;
   uint32_t first_index_;
   uint32_t vertex_offset_;
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

class MaterialComponent {
public:
  MaterialComponent();

  void setBaseColorTexture(const std::string& texturePath);
  void setMetallicRoughnessTexture(const std::string& texturePath);
  void setNormalTexture(const std::string& texturePath);
  void setEmissiveTexture(const std::string& texturePath);
  void setOcclusionTexture(const std::string& texturePath);

  void setBaseColorFactor(const glm::vec4& color);
  void setMetallicFactor(float metallic);
  void setRoughnessFactor(float roughness);

  const std::string& getBaseColorTexture() const;
  const std::string& getMetallicRoughnessTexture() const;
  const std::string& getNormalTexture() const;
  const std::string& getEmissiveTexture() const;
  const std::string& getOcclusionTexture() const;

  const glm::vec4& getBaseColorFactor() const;
  float getMetallicFactor() const;
  float getRoughnessFactor() const;

private:
  std::string baseColorTexture_;
  std::string metallicRoughnessTexture_;
  std::string normalTexture_;
  std::string emissiveTexture_;
  std::string occlusionTexture_;

  glm::vec4 baseColorFactor_;
  float metallicFactor_;
  float roughnessFactor_;
};

class transform_component {
public:
  transform_component(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale) :
    position_(position), rotation_(rotation), scale_(scale) {}

  // Getters and setters for position
  const glm::vec3& getPosition() const { return position_; }
  void setPosition(const glm::vec3& position) { position_ = position; }

  // Getters and setters for rotation (using quaternions)
  const glm::quat& getRotation() const { return rotation_; }
  void setRotation(const glm::quat& rotation) { rotation_ = rotation; }

  // Getters and setters for scale
  const glm::vec3& getScale() const { return scale_; }
  void setScale(const glm::vec3& scale) { scale_ = scale; }

  // Function to compute the model matrix
  glm::mat4 getModelMatrix() const {
    glm::mat4 translationMatrix = translate(position_);
    glm::mat4 rotationMatrix = mat4_cast(rotation_);
    glm::mat4 scaleMatrix = scale(scale_);
    return translationMatrix * rotationMatrix * scaleMatrix;
  }

private:
  glm::vec3 position_;
  glm::quat rotation_;
  glm::vec3 scale_;
};

// Component storage types
using entity_container = std::vector<entity>;
using mesh_container = std::vector<mesh_component>;
using camera_container = std::vector<CameraComponent>;
using light_container = std::vector<LightComponent>;
using material_container = std::vector<MaterialComponent>;
using transform_container = std::vector<transform_component>;

/**
 * @brief Class responsible for managing scenes, entities, and their components.
 */
class vk_scene_manager {
public:

  MaterialInstance default_data_;

  default_material default_material_;
  gpu_mesh_buffers mesh_buffers_;

  std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loaded_scenes_;

  void init(const vk_gpu& gpu, const resource_manager& resource_manager, render_engine& render_engine) {
    gpu_ = gpu;
    resource_manager_ = resource_manager;
    deletion_service_.init(gpu.device, gpu.allocator);

    load_scene_from_gltf("");
    init_default_data();
    init_renderables(render_engine);
  }

  void cleanup() {
       deletion_service_.flush();
  }

  void load_scene_from_gltf(const std::string& filename);

  entity create_entity() {
    const entity new_entity = {next_entity_id_++};
    entities_.push_back(new_entity);
    entity_to_entity_[new_entity] = entities_.size() - 1;
    return new_entity;
  }

  void add_mesh_component(const entity entity, std::vector<Vertex>& vertices,
                          std::vector<uint32_t>& indices);

  void add_camera_component(entity entity, const CameraComponent& camera) {
       cameras_.push_back(camera);
    entity_to_camera_[entity] = cameras_.size() - 1;
  }

  void add_light_component(entity entity, const LightComponent& light) {
             lights_.push_back(light);
    entity_to_light_[entity] = lights_.size() - 1;
  }

  void add_material_component(entity entity, const MaterialComponent& material) {
          materials_.push_back(material);
    entity_to_material_[entity] = materials_.size() - 1;
  }

  void add_transform_component(entity entity, const glm::vec3& position, const glm::quat& rotation,
                               const glm::vec3& scale = glm::vec3(1.f));

  const camera_container& get_cameras() const;

  const mesh_container& get_meshes() { return meshes_; }

  const light_container& get_lights() const;

  void set_parent(entity child, entity parent);

  const std::vector<entity>& get_children(entity entity) const;

  entity get_parent(entity entity) const;

  // Add more methods for other operations like updating, rendering, etc.

private:
  resource_manager resource_manager_;
  vk_deletion_service deletion_service_;

  vk_gpu gpu_; //TODO remove

  void init_default_data();
  void init_renderables(render_engine& render_engine);

  std::vector<Vertex> vertices_;
  std::vector<uint32_t> indices_;

  // Component containers
  entity_container entities_;
  mesh_container meshes_;
  camera_container cameras_;
  light_container lights_;
  material_container materials_;
  transform_container transforms_;

  // Entity to component mapping (for example, entity to its mesh component index)
  std::unordered_map<entity, size_t> entity_to_entity_;
  std::unordered_map<entity, size_t> entity_to_mesh_;
  std::unordered_map<entity, size_t> entity_to_camera_;
  std::unordered_map<entity, size_t> entity_to_light_;
  std::unordered_map<entity, size_t> entity_to_material_;
  std::unordered_map<entity, size_t> entity_to_transform_;

  // Add similar mappings for other components

  std::unordered_map<entity, entity_data> entity_hierarchy_;

  // Next available entity ID
  entity next_entity_id_ = 0;
};
