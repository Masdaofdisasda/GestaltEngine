
#include <scene_manager.h>

#include "vk_types.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>

#include <glm/gtx/matrix_decompose.hpp>

void scene_manager::init(const vk_gpu& gpu,const std::shared_ptr <resource_manager>& resource_manager) {
  gpu_ = gpu;
  resource_manager_ = resource_manager;

  auto [entity, root_node] = create_entity();
  root_node.get().name = "root";

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

  const size_t camera_index = resource_manager_->get_database().add_camera(camera_data{});
  resource_manager_->get_database().add_camera(create_entity().first, camera_component{camera_index});

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
    const auto [entity, node_component] = create_entity();

    if (node.meshIndex.has_value()) {
      add_mesh_component(entity, mesh_offset + *node.meshIndex);
    }

    if (std::holds_alternative<fastgltf::Node::TRS>(node.transform)) {
      const auto& trs = std::get<fastgltf::Node::TRS>(node.transform);
      glm::vec3 translation(trs.translation[0], trs.translation[1], trs.translation[2]);
      glm::quat rotation(trs.rotation[3], trs.rotation[0], trs.rotation[1], trs.rotation[2]);
      glm::vec3 scale(trs.scale[0], trs.scale[1], trs.scale[2]);

      set_transform_component(entity, translation, rotation, scale);
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

size_t scene_manager::create_material(pbr_material& config,
                                     const std::string& name) const {
  const size_t material_id = resource_manager_->get_database().get_materials_size();

  const std::string key = name.empty() ? "material_" + std::to_string(material_id) : name;
  resource_manager_->write_material(config, material_id);

  resource_manager_->get_database().add_material(
      material{.name = key, .config = config});

  fmt::print("creating material {}, mat_id {}\n", name, material_id);
  return material_id;
}

std::pair<entity, std::reference_wrapper<node_component>> scene_manager::create_entity() {
  const entity new_entity = {next_entity_id_++};

  const node_component node = {
    .name = "entity_" + std::to_string(new_entity),
  };

  resource_manager_->get_database().add_node(new_entity, node);

  set_transform_component(new_entity, glm::vec3(0));

  fmt::print("created entity {}\n", new_entity);

  return std::make_pair(new_entity,
                        std::ref(resource_manager_->get_database().get_node_component(new_entity)->get()));
}

void scene_manager::set_transform_component(const entity entity, const glm::vec3& position,
                                            const glm::quat& rotation, const glm::vec3& scale) {
  assert(entity != invalid_entity);

  std::optional<std::reference_wrapper<transform_component>> transform_optional = resource_manager_
      ->get_database().get_transform_component(entity);

  if (transform_optional.has_value()) {
      auto& transform = transform_optional->get();

       transform = transform_component(position, rotation, scale);
  } else {
       resource_manager_->get_database().add_transform(
           entity, transform_component(position, rotation, scale));
  }

  auto& transform = resource_manager_->get_database().get_transform_component(entity)->get();

  if (transform.matrix == no_component) {
    transform.matrix = resource_manager_->get_database().add_matrix(transform.get_model_matrix());
  } else {
    resource_manager_->get_database().set_matrix(transform.matrix, transform.get_model_matrix());
  }

  transform.is_dirty = false;
}

node_component& scene_manager::get_root_node() {
  return resource_manager_->get_database().get_node_component(get_root_entity()).value();
}

void scene_manager::add_mesh_component(const entity entity, const size_t mesh_index) {
  assert(entity != invalid_entity);

  resource_manager_->get_database().add_mesh(entity, mesh_component{mesh_index});
}

void scene_manager::add_camera_component(const entity entity, const camera_component& camera) {
  assert(entity != invalid_entity);

  resource_manager_->get_database().add_camera(entity, camera);
}

entity scene_manager::create_light(const light_component& light) {
  auto [entity, node] = create_entity();

  add_to_root(entity, node);

  size_t current_count = resource_manager_->get_database().get_lights(light.type).size();
  if (current_count == resource_manager_->get_database().max_lights(light.type)) {
    fmt::print("max lights reached\n");
    return -1;
  }

  if (light.type == light_type::point || light.type == light_type::spot) {
    set_transform_component(entity, light.position);
  }

  resource_manager_->get_database().add_light(entity, light);
  const size_t matrix_id = resource_manager_->get_database().add_light_view_proj(glm::mat4(1.0));
  resource_manager_->get_database().get_light_component(entity).value().get()
  .light_view_projections.push_back(matrix_id);

  return entity;
}

void scene_manager::add_to_root(entity entity, node_component& node) {
  assert(entity != invalid_entity);
  get_root_node().children.push_back(entity);
  node.parent = get_root_entity();
}

void scene_manager::update_scene() {

  for (const auto& system : systems_) {
       system->update();
  }

  constexpr glm::mat4 identity = glm::mat4(1.0f);

  traverse_scene(get_root_entity(), identity);
 }

void scene_manager::traverse_scene(const entity entity, const glm::mat4& parent_transform) {
  assert(entity != invalid_entity);
  const auto& node = resource_manager_->get_database().get_node_component(entity).value().get();
  if (!node.visible) {
    return;
  }

  const auto& transform = resource_manager_->get_database().get_transform_component(entity)->get();
  if (transform.is_dirty) {

    resource_manager_->get_database().set_matrix(entity, transform.get_model_matrix());
    transform.is_dirty = false;
  }
  const glm::mat4 world_transform = parent_transform * resource_manager_->get_database().get_matrix(transform.matrix);

  const auto& mesh_component = resource_manager_->get_database().get_mesh_component(entity);
  if (mesh_component.has_value()) {
    const auto& mesh = resource_manager_->get_database().get_mesh(mesh_component->get().mesh);

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

  for (const auto& childEntity : node.children) {
    traverse_scene(childEntity, world_transform);
  }
}
