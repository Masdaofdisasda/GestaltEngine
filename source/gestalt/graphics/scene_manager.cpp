
#include <scene_manager.h>

#include "vk_types.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>

#include <glm/gtx/matrix_decompose.hpp>

void scene_manager::init(const vk_gpu& gpu,const std::shared_ptr <resource_manager>& resource_manager) {
  gpu_ = gpu;
  resource_manager_ = resource_manager;

  scene_graph_.reserve(1000);
  get_scene_object_by_entity(create_entity().entity).value().get().name = "root";

  asset_loader_->init(gpu_, resource_manager_);

  resource_manager_->init_default_data();

  size_t mesh_offset = resource_manager_->get_database().get_meshes_size();

  //const auto nodes = asset_loader_->load_scene_from_gltf(R"(..\..\assets\Models\MetalRoughSpheres\glTF-Binary\MetalRoughSpheres.glb)");
  //const auto nodes = asset_loader_->load_scene_from_gltf(R"(..\..\assets\Models\MetalRoughSpheresNoTextures\glTF-Binary\MetalRoughSpheresNoTextures.glb)");
  const auto nodes = asset_loader_->load_scene_from_gltf(R"(..\..\assets\sponza_pestana.glb)");
  //const auto nodes = asset_loader_->load_scene_from_gltf(R"(..\..\assets\Models\DamagedHelmet\glTF-Binary\DamagedHelmet.glb)");
  //const auto nodes = asset_loader_->load_scene_from_gltf(R"(..\..\assets\Bistro.glb)");
  //const auto nodes = asset_loader_->load_scene_from_gltf(R"(..\..\assets\bmw.glb)");
  //const auto nodes = asset_loader_->load_scene_from_gltf(R"(..\..\assets\structure.glb)");

  build_scene_graph(nodes, mesh_offset);

  resource_manager_->load_and_process_cubemap(R"(..\..\assets\san_giuseppe_bridge_4k.hdr)");

  //TODO add lights from gltf to resource manager

  resource_manager_->get_database().add_camera(camera_component());

  create_light(light_component::DirectionalLight(glm::vec3(1.f, 0.957f, 0.917f), 20.f,
                                                 glm::vec3(-0.216, 0.941, -0.257)));
  create_light(light_component::PointLight(glm::vec3(1.0f), 5.0f, glm ::vec3(0.0,6.0, 0.0)));

  systems_.push_back(std::make_unique<light_system>());

  for (const auto& system : systems_) {
       system->init(gpu_, resource_manager_);
  }
}

void scene_manager::create_entities(std::vector<fastgltf::Node> nodes, const size_t& mesh_offset) {
  for (fastgltf::Node& node : nodes) {
    const entity_component& new_node = create_entity();

    if (node.meshIndex.has_value()) {
      add_mesh_component(new_node.entity, mesh_offset + *node.meshIndex);
    }

    if (std::holds_alternative<fastgltf::Node::TRS>(node.transform)) {
      const auto& trs = std::get<fastgltf::Node::TRS>(node.transform);
      glm::vec3 translation(trs.translation[0], trs.translation[1], trs.translation[2]);
      glm::quat rotation(trs.rotation[3], trs.rotation[0], trs.rotation[1], trs.rotation[2]);
      glm::vec3 scale(trs.scale[0], trs.scale[1], trs.scale[2]);

      set_transform_component(new_node.entity, translation, rotation, scale);
    }
  }
}

void scene_manager::build_hierarchy(std::vector<fastgltf::Node> nodes, const size_t& node_offset) {
  for (int i = 0; i < nodes.size(); i++) {
    fastgltf::Node& node = nodes[i];
    auto& scene_object = get_scene_object_by_entity(node_offset + i).value().get();

    for (auto& c : node.children) {
      auto& child = get_scene_object_by_entity(node_offset + c).value().get();
      scene_object.children.push_back(child.entity);
      child.parent = scene_object.entity;
    }
  }
}

void scene_manager::link_orphans_to_root() {
  for (auto& node : scene_graph_) {
    if (node.entity == get_root().entity) {
           continue;
    }

    if (node.parent == invalid_entity) {
           get_root().children.push_back(node.entity);
           node.parent = get_root().entity;
    }
  }
}

void scene_manager::build_scene_graph(const std::vector<fastgltf::Node>& nodes, const size_t& mesh_offset) {
  const size_t node_offset = scene_graph_.size();

  create_entities(nodes, mesh_offset);
  build_hierarchy(nodes, node_offset);
  link_orphans_to_root();
}

void scene_manager::cleanup() {
}

size_t scene_manager::create_material(pbr_material& config,
                                     const std::string& name) const {
  const size_t material_id = resource_manager_->get_database().get_materials_size();

  const std::string key = name.empty() ? "material_" + std::to_string(material_id) : name;
  resource_manager_->write_material(config, material_id);

  resource_manager_->get_database().add_material(material_component{.name = key, .config = config});

  fmt::print("creating material {}, mat_id {}\n", name, material_id);
  return material_id;
}

entity_component& scene_manager::create_entity() {
  const entity new_entity = {next_entity_id_++};

  const entity_component object = {
    .name = "entity_" + std::to_string(new_entity),
    .entity = new_entity,
  };
  scene_graph_.push_back(object);

  set_transform_component(new_entity, glm::vec3(0));

  fmt::print("created entity {}\n", new_entity);

  return scene_graph_.at(new_entity);
}

void scene_manager::set_transform_component(entity entity, const glm::vec3& position,
                                            const glm::quat& rotation, const glm::vec3& scale) {
  assert(entity != invalid_entity);
  auto& scene_object = get_scene_object_by_entity(entity).value().get();
  scene_object.transform = resource_manager_->get_database().add_transform(
      transform_component(position, rotation, scale));
  resource_manager_->get_database().add_matrix(
      resource_manager_->get_database().get_transform(scene_object.transform).get_model_matrix());
  resource_manager_->get_database().get_transform(scene_object.transform).is_dirty = false;
}

void scene_manager::add_mesh_component(const entity entity, size_t mesh_index) {
  assert(entity != invalid_entity);

  entity_component& object = get_scene_object_by_entity(entity).value().get();
  object.mesh = mesh_index;
}

void scene_manager::add_camera_component(entity entity, const camera_component& camera) {
  entity_component& object = get_scene_object_by_entity(entity).value().get();
  object.camera = resource_manager_->get_database().add_camera(camera);
}

size_t scene_manager::create_light(const light_component& light) {
  auto& light_node = create_entity();
  get_root().children.push_back(light_node.entity);
  light_node.parent = get_root().entity;

  size_t current_count = resource_manager_->get_database().get_lights(light.type).size();
  if (current_count == resource_manager_->get_database().max_lights(light.type)) {
    fmt::print("max lights reached\n");
    return -1;
  }

  if (light.type == light_type::point || light.type == light_type::spot) {
    set_transform_component(light_node.entity, light.position);
  }

  light_node.light = resource_manager_->get_database().add_light(light);
  const size_t matrix_id = resource_manager_->get_database().add_light_view_proj(glm::mat4(1.0));
  resource_manager_->get_database().get_light(light_node.light).light_view_projections.push_back(matrix_id);

  return light_node.entity;
}

const std::vector<entity>& scene_manager::get_children(entity entity) {
  assert(entity != invalid_entity);
  return get_scene_object_by_entity(entity).value().get().children;
}

 std::optional<std::reference_wrapper<entity_component>> scene_manager::get_scene_object_by_entity(entity entity) {
  assert(entity != invalid_entity);

  if (scene_graph_.size() >= entity) {
    return scene_graph_.at(entity);
  }

  fmt::print("could not find scene object for entity {}", entity);
  return std::nullopt;
 }


void scene_manager::update_scene() {

  for (const auto& system : systems_) {
       system->update(scene_graph_);
  }

  constexpr glm::mat4 identity = glm::mat4(1.0f);

  traverse_scene(get_root().entity, identity);
 }

void scene_manager::traverse_scene(const entity entity, const glm::mat4& parent_transform) {
  assert(entity != invalid_entity);
  const auto& object = get_scene_object_by_entity(entity).value().get();
  if (!object.visible) {
    return;
  }

  const auto& transform = resource_manager_->get_database().get_transform(object.transform);
  if (transform.is_dirty) {

    resource_manager_->get_database().set_matrix(object.transform, transform.get_model_matrix());
    transform.is_dirty = false;
  }
  const glm::mat4 world_transform = parent_transform * resource_manager_->get_database().get_matrix(object.transform);

  if (object.is_valid() && object.has_mesh()) {
    const auto& mesh = resource_manager_->get_database().get_mesh(object.mesh);

    for (const auto surface_index : mesh.surfaces) {
      const auto& surface = resource_manager_->get_database().get_surface(surface_index);

      const auto& material = resource_manager_->get_database().get_material(surface.material);

      render_object def;
      def.index_count = surface.index_count;
      def.first_index = surface.first_index;
      def.material = surface.material;
      def.bounds = surface.bounds;
      def.transform = world_transform;
      def.vertex_buffer_address = resource_manager_->scene_geometry_.vertexBufferAddress;

      if (material.config.transparent) {
        resource_manager_->main_draw_context_.transparent_surfaces.push_back(def);
      } else {
        resource_manager_->main_draw_context_.opaque_surfaces.push_back(def);
      }
    }
  }

  // Process child nodes recursively, passing the current world transform
  for (const auto& childEntity : object.children) {
    traverse_scene(childEntity, world_transform);
  }
}
