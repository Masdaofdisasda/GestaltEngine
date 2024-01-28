#pragma once

#include <filesystem>
#include <unordered_map>

#include "scene_components.h"

#include "materials.h"
#include "resource_manager.h"
#include "vk_descriptors.h"

// forward declaration
class render_engine;


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

  AllocatedBuffer material_data_buffer_;
  DescriptorAllocatorGrowable descriptorPool;

  void init(const vk_gpu& gpu, const resource_manager& resource_manager,
            const gltf_metallic_roughness& material);
  void cleanup();

  void load_scene_from_gltf(const std::string& file_path);
  void update_scene(draw_context& draw_context);
  void traverse_scene(entity entity, const glm::mat4& parent_transform, draw_context& draw_context);

  entity_component& create_entity();
  size_t create_surface(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
  size_t create_mesh(std::vector<size_t> surfaces, const std::string& name);
  void add_mesh_component(entity entity, size_t mesh_index);
  void add_camera_component(entity entity, const CameraComponent& camera);
  void add_light_component(entity entity, const LightComponent& light);
  size_t create_material(MaterialPass pass_type,
                    const gltf_metallic_roughness::MaterialResources& resources,
                    const pbr_config& config = pbr_config{},
                    const std::string& name = "");
  void add_material_component(size_t surface, size_t material);
  void set_transform_component(entity entity, const glm::vec3& position,
                               const glm::quat& rotation = glm::quat(0.f, 0.f, 0.f, 0.f),
                               const glm::vec3& scale = glm::vec3(1.f));

  const camera_container& get_cameras() { return cameras_; }
  material_component& get_material(const size_t material) { return materials_[material]; }
  mesh_surface& get_surface(const size_t surface) { return surfaces_[surface]; }
  mesh_component& get_mesh(const size_t mesh) { return meshes_[mesh]; }
  transform_component& get_transform(const size_t transform) { return transforms_[transform]; }
  const light_container& get_lights() { return lights_; }

  const entity_component& get_root() { return root_; }
  const std::vector<entity>& get_children(entity entity);
  std::optional<std::reference_wrapper<entity_component>> get_scene_object_by_entity(entity entity);


private:
  resource_manager resource_manager_;
  vk_deletion_service deletion_service_ = {};
  gltf_metallic_roughness gltf_material_ = {};

  vk_gpu gpu_ = {}; //TODO remove

  void init_default_data();

  // Data containers
  std::vector<Vertex> vertices_;
  std::vector<uint32_t> indices_;
  std::vector<mesh_surface> surfaces_;
  std::vector<glm::mat4> matrices_;
  std::vector<AllocatedImage> images_;
  std::vector<VkSampler> samplers_;

  // Component containers
  mesh_container meshes_;
  camera_container cameras_;
  light_container lights_;
  material_container materials_;
  transform_container transforms_;

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

  std::vector<entity_component> scene_graph_;
  entity_component root_ = {"root", invalid_entity, {}, invalid_entity};

  // Next available entity ID
  entity next_entity_id_ = 0;
};
