#pragma once

#include <filesystem>

#include "asset_loader.h"
#include "scene_components.h"

#include "resource_manager.h"
#include "scene_systems.h"

class component_archetype_factory {
  std::shared_ptr<resource_manager> resource_manager_;

  entity next_entity_id_ = 0;

public:
  void init(const std::shared_ptr<resource_manager>& resource_manager);

  entity create_entity();
  std::pair<entity, std::reference_wrapper<node_component>> create_entity_node(std::string node_name
                                                                               = "");
  void add_mesh_component(entity entity, size_t mesh_index);
  void add_camera_component(entity entity, const camera_component& camera);

  entity create_directional_light(const glm::vec3& color, const float intensity,
                                  const glm::vec3& direction, entity parent = 0);
  entity create_spot_light(const glm::vec3& color, const float intensity,
                           const glm::vec3& direction, const glm::vec3& position,
                           const float innerCone, const float outerCone, entity parent = 0);
  entity create_point_light(const glm::vec3& color, const float intensity,
                            const glm::vec3& position, entity parent = 0);

  void link_entity_to_parent(entity child, entity parent);
  void update_transform_component(const unsigned entity, const glm::vec3& position,
                                  const glm::quat& rotation = glm::quat(0.f, 0.f, 0.f, 0.f),
                                  const float& scale = 1.f);
  void create_transform_component(const unsigned entity, const glm::vec3& position,
                                  const glm::quat& rotation = glm::quat(0.f, 0.f, 0.f, 0.f),
                                  const float& scale = 1.f) const;
};

/**
 * @brief Class responsible for managing scenes, entities, and their components.
 */
class scene_manager {
  vk_gpu gpu_ = {};
  std::shared_ptr<resource_manager> resource_manager_;
  std::unique_ptr<asset_loader> asset_loader_ = std::make_unique<asset_loader>();
  std::unique_ptr<component_archetype_factory> component_factory_ = std::make_unique<component_archetype_factory>();

  std::unique_ptr<scene_system> light_system_;
  std::unique_ptr<scene_system> transform_system_;
  std::unique_ptr<scene_system> render_system_;


  void build_scene_graph(const std::vector<fastgltf::Node>& nodes, const size_t& mesh_offset);
  void create_entities(std::vector<fastgltf::Node> nodes, const size_t& mesh_offset);
  void build_hierarchy(std::vector<fastgltf::Node> nodes, const size_t& node_offset);
  void link_orphans_to_root();
  const entity root_entity_ = 0;

  void load_scene(std::string path);
  std::string scene_path_ = "";

public:
  void init(const vk_gpu& gpu, const std::shared_ptr<resource_manager>& resource_manager);
  void cleanup();

  void update_scene();

  void request_scene(const std::string& path);
  size_t create_material(pbr_material& config, const std::string& name = "") const;
  node_component& get_root_node();
  uint32_t get_root_entity() { return root_entity_; }
  void add_to_root(entity entity, node_component& node);
};
