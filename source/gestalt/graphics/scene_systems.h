#pragma once

#include "resource_manager.h"
#include "vk_gpu.h"

class scene_system {
public:
  void init(const vk_gpu& gpu, const std::shared_ptr<resource_manager>& resource_manager) {
    gpu_ = gpu;
    resource_manager_ = resource_manager;

    prepare();
  }
  virtual ~scene_system() = default;

  virtual void update() = 0;
  virtual void cleanup() = 0;

protected:
  virtual void prepare() = 0;

  vk_gpu gpu_ = {};
  std::shared_ptr<resource_manager> resource_manager_;
};

class light_system final : public scene_system {
  descriptor_writer writer;

  glm::mat4 calculate_sun_view_proj(const light_component& light);
  void update_directional_lights(const std::vector<std::reference_wrapper<light_component>>& lights);
  void update_point_lights(const std::vector<std::reference_wrapper<light_component>>& lights);

public:
  void prepare() override;
  void update() override;
  void cleanup() override;
};

class transform_system final : public scene_system {
  static glm::mat4 get_model_matrix(const transform_component& transform);

  void mark_children_dirty(entity entity);
  void mark_as_dirty(entity entity);
  void update_aabb(unsigned entity, const glm::mat4& parent_transform);
  void mark_parent_dirty(entity entity);

public:
  void prepare() override;
  void update() override;
  void cleanup() override;
};

class render_system final : public scene_system {
  size_t meshes_ = 0;
  void traverse_scene(const entity entity, const glm::mat4& parent_transform);

public:
  void prepare() override;
  void update() override;
  void cleanup() override {}
};

class hierarchy_system final : public scene_system {
   void build_scene_graph(const std::vector<fastgltf::Node>& nodes, const size_t& mesh_offset);
  void create_entities(std::vector<fastgltf::Node> nodes, const size_t& mesh_offset);
  void build_hierarchy(std::vector<fastgltf::Node> nodes, const size_t& node_offset);
  void link_orphans_to_root();

public:
  void prepare() override;
  void update() override;
  void cleanup() override;
};