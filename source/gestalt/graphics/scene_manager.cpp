
#include <scene_manager.h>

#include "vk_types.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>

#include <glm/gtx/matrix_decompose.hpp>

void scene_manager::init(const vk_gpu& gpu,
                         const std::shared_ptr<resource_manager>& resource_manager) {
  gpu_ = gpu;
  resource_manager_ = resource_manager;

  component_factory_->init(resource_manager_);
  asset_loader_->init(gpu_, resource_manager_);

  resource_manager_->init_default_data();

  resource_manager_->load_and_process_cubemap(R"(..\..\assets\san_giuseppe_bridge_4k.hdr)");

  // TODO add lights from gltf to resource manager

  const size_t camera_index = resource_manager_->get_database().add_camera(camera_data{});
  resource_manager_->get_database().add_camera(component_factory_->create_entity_node().first,
                                               camera_component{camera_index});

  component_factory_->create_directional_light(glm::vec3(1.f, 0.957f, 0.917f), 5.f,
                                               glm::vec3(-0.216, 0.941, -0.257), get_root_entity());
  component_factory_->create_point_light(glm::vec3(1.0f), 5.0f, glm ::vec3(0.0, 6.0, 0.0),
                                         get_root_entity());

  light_system_ = std::make_unique<light_system>();
  light_system_->init(gpu, resource_manager);
  transform_system_ = std::make_unique<transform_system>();
  transform_system_->init(gpu, resource_manager);
  render_system_ = std::make_unique<render_system>();
  render_system_->init(gpu, resource_manager);

}


void scene_manager::load_scene(const std::string& path) {
  size_t mesh_offset = resource_manager_->get_database().get_meshes_size();
  const auto nodes = asset_loader_->load_scene_from_gltf(path);
  build_scene_graph(nodes, mesh_offset);
}

void scene_manager::create_entities(std::vector<fastgltf::Node> nodes, const size_t& mesh_offset) {
  for (fastgltf::Node& node : nodes) {
    const auto [entity, node_component] = component_factory_->create_entity_node(node.name.c_str());

    if (node.meshIndex.has_value()) {
      component_factory_->add_mesh_component(entity, mesh_offset + *node.meshIndex);
    }

    if (std::holds_alternative<fastgltf::TRS>(node.transform)) {
      const auto& trs = std::get<fastgltf::TRS>(node.transform);

      glm::vec3 translation(trs.translation[0], trs.translation[1], trs.translation[2]);
      glm::quat rotation(trs.rotation[3], trs.rotation[0], trs.rotation[1], trs.rotation[2]);
      float scale = (trs.scale[0] + trs.scale[1] + trs.scale[2]) / 3.f;//TODO handle non-uniform scale

      component_factory_->update_transform_component(entity, translation, rotation, scale);
    }
  }
}

void scene_manager::build_hierarchy(std::vector<fastgltf::Node> nodes, const size_t& node_offset) {
  for (int i = 0; i < nodes.size(); i++) {
    fastgltf::Node& node = nodes[i];
    entity parent_entity = node_offset + i;
    auto& scene_object = resource_manager_->get_database().get_node_component(parent_entity).value().get();

    for (auto& c : node.children) {
      entity child_entity = node_offset + c;
      auto& child
          = resource_manager_->get_database().get_node_component(child_entity).value().get();
      scene_object.children.push_back(child_entity);
      child.parent = parent_entity;
    }
  }
}


void scene_manager::link_orphans_to_root() {
  for (auto& [entity, node] : resource_manager_->get_database().get_scene_graph()) {
    if (entity == get_root_entity()) {
      continue;
    }

    if (node.parent == invalid_entity) {
      get_root_node().children.push_back(entity);
      node.parent = get_root_entity();
    }
  }
}

void scene_manager::build_scene_graph(const std::vector<fastgltf::Node>& nodes, const size_t& mesh_offset) {
  const size_t node_offset = resource_manager_->get_database().get_scene_graph().size();

  create_entities(nodes, mesh_offset);
  build_hierarchy(nodes, node_offset);
  link_orphans_to_root();
}

void scene_manager::cleanup() {
}

entity component_archetype_factory::create_entity() {
  return next_entity_id_++;
} 

std::pair<entity, std::reference_wrapper<node_component>>
component_archetype_factory::create_entity_node(std::string node_name) {
  const entity new_entity = create_entity();

  if (node_name.empty()) {
    node_name = "entity_" + std::to_string(new_entity);
  }
  const node_component node = {
      .name = node_name + "_" + std::to_string(new_entity),
  };

  create_transform_component(new_entity, glm::vec3(0));
  resource_manager_->get_database().add_node(new_entity, node);


  fmt::print("created entity {}\n", new_entity);

  return std::make_pair(
      new_entity,
      std::ref(resource_manager_->get_database().get_node_component(new_entity)->get()));
}

void component_archetype_factory::create_transform_component(const entity entity,
                                                          const glm::vec3& position,
                                                          const glm::quat& rotation,
                                                          const float& scale) const {
  resource_manager_->get_database().add_transform(entity,
                                                  transform_component(position, rotation, scale));
  auto& transform = resource_manager_->get_database().get_transform_component(entity)->get();
  transform.matrix = resource_manager_->get_database().add_matrix(glm::mat4(1.0));
}

void component_archetype_factory::update_transform_component(const entity entity,
                                                          const glm::vec3& position,
                                                          const glm::quat& rotation,
                                                          const float& scale) {
  const std::optional<std::reference_wrapper<transform_component>> transform_optional
      = resource_manager_->get_database().get_transform_component(entity);

  if (transform_optional.has_value()) {
    auto& transform = transform_optional->get();
    
    transform.position = position;
    transform.rotation = rotation;
    transform.scale = scale;
    transform.is_dirty = true;
  }
}

node_component& scene_manager::get_root_node() {
  return resource_manager_->get_database().get_node_component(get_root_entity()).value();
}

void component_archetype_factory::add_mesh_component(const entity entity, const size_t mesh_index) {
  assert(entity != invalid_entity);

  resource_manager_->get_database().add_mesh(entity, mesh_component{mesh_index});
}

void component_archetype_factory::add_camera_component(const entity entity,
                                                       const camera_component& camera) {
  assert(entity != invalid_entity);

  resource_manager_->get_database().add_camera(entity, camera);
}

entity component_archetype_factory::create_directional_light(const glm::vec3& color,
                                                             const float intensity,
                                                             const glm::vec3& direction,
                                                             entity parent) {
  auto [entity, node] = create_entity_node("directional_light");
  link_entity_to_parent(entity, parent);

  auto& transform = resource_manager_->get_database().get_transform_component(entity).value().get();
  transform.rotation = glm::quat(glm::lookAt(glm::vec3(0), direction, glm::vec3(0, 1, 0)));

  const light_component light{
      .type = light_type::directional,
      .color = color,
      .intensity = intensity,
  };

  resource_manager_->get_database().add_light(entity, light);
  const size_t matrix_id = resource_manager_->get_database().add_light_view_proj(glm::mat4(1.0));
  resource_manager_->get_database()
      .get_light_component(entity)
      .value()
      .get()
      .light_view_projections.push_back(matrix_id);

  return entity;
}

entity component_archetype_factory::create_spot_light(const glm::vec3& color, const float intensity,
                                                         const glm::vec3& direction, const glm::vec3& position,
                                                         const float innerCone, const float outerCone, entity parent) {
  auto [entity, node] = create_entity_node("spot_light");
  link_entity_to_parent(entity, parent);

  auto& transform = resource_manager_->get_database().get_transform_component(entity).value().get();
  transform.rotation = glm::quat(glm::lookAt(glm::vec3(0), direction, glm::vec3(0, 1, 0)));
  transform.position = position;

   const light_component light{
       .type = light_type::spot,
       .color = color,
       .intensity = intensity,
       .inner_cone = innerCone,
       .outer_cone = outerCone,
   };

  resource_manager_->get_database().add_light(entity, light);
  const size_t matrix_id = resource_manager_->get_database().add_light_view_proj(glm::mat4(1.0));
  resource_manager_->get_database()
      .get_light_component(entity)
      .value()
      .get()
      .light_view_projections.push_back(matrix_id);

  return entity;
}

entity component_archetype_factory::create_point_light(const glm::vec3& color,
                                                       const float intensity,
                                                       const glm::vec3& position, entity parent) {
  auto [entity, node] = create_entity_node("point_light");
  link_entity_to_parent(entity, parent);

  auto& transform = resource_manager_->get_database().get_transform_component(entity).value().get();
  transform.position = position;

  const light_component light{
      .type = light_type::point,
      .color = color,
      .intensity = intensity,
  };

  resource_manager_->get_database().add_light(entity, light);
  for (int i = 0; i < 6; i++) {
    const size_t matrix_id = resource_manager_->get_database().add_light_view_proj(glm::mat4(1.0));
    resource_manager_->get_database()
        .get_light_component(entity)
        .value()
        .get()
        .light_view_projections.push_back(matrix_id);
  }
  return entity;
}

void component_archetype_factory::link_entity_to_parent(const entity child, const entity parent) {
  if (child == parent) {
       return;
  }

  const auto& parent_node = resource_manager_->get_database().get_node_component(parent);
  const auto& child_node = resource_manager_->get_database().get_node_component(child);

  if (parent_node.has_value() && child_node.has_value()) {
    parent_node->get().children.push_back(child);
    child_node->get().parent = parent;
  }
}

void scene_manager::add_to_root(entity entity, node_component& node) {
  assert(entity != invalid_entity);
  get_root_node().children.push_back(entity);
  node.parent = get_root_entity();
}

void scene_manager::update_scene() {

  if (!scene_path_.empty()) {
    load_scene(scene_path_);
    scene_path_.clear();
  }

  light_system_->update();
  transform_system_->update();
  render_system_->update();
 }

void scene_manager::request_scene(const std::string& path) {
  scene_path_ = path;
}

void component_archetype_factory::init(const std::shared_ptr<resource_manager>& resource_manager) {
  resource_manager_ = resource_manager;

  auto [entity, root_node] = create_entity_node();
  root_node.get().name = "root";
}
