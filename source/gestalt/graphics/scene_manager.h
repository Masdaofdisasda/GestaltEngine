#pragma once

#include <filesystem>

#include "asset_loader.h"
#include "scene_components.h"

#include "resource_manager.h"
#include "scene_systems.h"

/**
 * @brief Class responsible for managing scenes, entities, and their components.
 */
class scene_manager {
  vk_gpu gpu_ = {};
  std::shared_ptr<resource_manager> resource_manager_;
  std::unique_ptr<asset_loader> asset_loader_ = std::make_unique<asset_loader>();
  std::vector<std::unique_ptr<scene_system>> systems_;

  void build_scene_graph(const std::vector<fastgltf::Node>& nodes, const size_t& mesh_offset);
  void create_entities(std::vector<fastgltf::Node> nodes, const size_t& mesh_offset);
  void build_hierarchy(std::vector<fastgltf::Node> nodes, const size_t& node_offset);
  void link_orphans_to_root();

  entity next_entity_id_ = 0;

public:
  void init(const vk_gpu& gpu, const std::shared_ptr<resource_manager>& resource_manager);
  void cleanup();

  void update_scene();
  void traverse_scene(entity entity, const glm::mat4& parent_transform);

  std::pair<entity, std::reference_wrapper<node_component>> create_entity();
  void add_mesh_component(entity entity, size_t mesh_index);
  void add_camera_component(entity entity, const camera_component& camera);
  size_t create_material(
      pbr_material& config,
      const std::string& name = "") const;
  entity create_light(const light_component& light);
  void set_transform_component(entity entity, const glm::vec3& position,
                               const glm::quat& rotation = glm::quat(0.f, 0.f, 0.f, 0.f),
                               const glm::vec3& scale = glm::vec3(1.f));

  node_component& get_root_node();
  uint32_t get_root_entity() { return 0; }
  void add_to_root(entity entity, node_component& node);
};
