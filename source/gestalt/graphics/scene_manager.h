#pragma once

#include <filesystem>

#include "asset_loader.h"
#include "scene_components.h"

#include "resource_manager.h"
#include "vk_descriptors.h"

/**
 * @brief Class responsible for managing scenes, entities, and their components.
 */
class scene_manager {
  vk_gpu gpu_ = {};
  std::shared_ptr<resource_manager> resource_manager_;
  std::unique_ptr<asset_loader> asset_loader_ = std::make_unique<asset_loader>();

  void init_default_data() const; // todo use resource manager
  void build_scene_graph(const std::vector<fastgltf::Node>& nodes, const size_t& mesh_offset);
  void create_entities(std::vector<fastgltf::Node> nodes, const size_t& node_offset);
  void build_hierarchy(std::vector<fastgltf::Node> nodes, const size_t& node_offset);
  void link_orphans_to_root();

  std::vector<entity_component> scene_graph_;
  entity_component root_ = {"root", invalid_entity, {}, invalid_entity};

  // Next available entity ID
  entity next_entity_id_ = 0;

public:
  void init(const vk_gpu& gpu, const std::shared_ptr<resource_manager>& resource_manager);
  void cleanup();

  void load_environment_map(const std::string& file_path) const;
  void update_scene(draw_context& draw_context);
  void traverse_scene(entity entity, const glm::mat4& parent_transform, draw_context& draw_context);

  entity_component& create_entity();
  void add_mesh_component(entity entity, size_t mesh_index);
  void add_camera_component(entity entity, const CameraComponent& camera);
  void add_light_component(entity entity, const LightComponent& light);
  size_t create_material(
      const pbr_material& config = pbr_material{},
      const std::string& name = "") const;
  void set_transform_component(entity entity, const glm::vec3& position,
                               const glm::quat& rotation = glm::quat(0.f, 0.f, 0.f, 0.f),
                               const glm::vec3& scale = glm::vec3(1.f));

  const entity_component& get_root() { return root_; } //TODO add to scene graph
  const std::vector<entity>& get_children(entity entity);
  std::optional<std::reference_wrapper<entity_component>> get_scene_object_by_entity(entity entity);
};
