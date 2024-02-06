
#include <scene_manager.h>

#include "vk_types.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>

#include <glm/gtx/matrix_decompose.hpp>

void scene_manager::init(const vk_gpu& gpu,const std::shared_ptr <resource_manager>& resource_manager) {
  gpu_ = gpu;
  resource_manager_ = resource_manager;
  asset_loader_->init(gpu_, resource_manager_);

  init_default_data();

  size_t mesh_offset = resource_manager_->get_database().get_meshes_size();

  //load_scene_from_gltf(R"(..\..\assets\Models\MetalRoughSpheres\glTF-Binary\MetalRoughSpheres.glb)");
  const auto nodes = asset_loader_->load_scene_from_gltf(R"(..\..\assets\sponza_pestana.glb)");
  //const auto nodes = asset_loader_->load_scene_from_gltf(R"(..\..\assets\awesome-3d-meshes\McGuire\Amazon Lumberyard Bistro\gltf\Bistro.glb)");
  //load_scene_from_gltf(R"(..\..\assets\structure.glb)");

  build_scene_graph(nodes, mesh_offset);

  load_environment_map(R"(..\..\assets\san_giuseppe_bridge_4k.hdr)"); 
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
      auto child = get_scene_object_by_entity(node_offset + c).value().get();
      scene_object.children.push_back(child.entity);
      child.parent = scene_object.entity;
    }
  }
}

void scene_manager::link_orphans_to_root() {
  for (auto& node : scene_graph_) {
    if (node.parent == invalid_entity) {
      root_.children.push_back(node.entity);
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

void scene_manager::load_environment_map(const std::string& file_path) const {
  resource_manager_->load_and_process_cubemap(file_path);
}

size_t scene_manager::create_material(const pbr_material& config,
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

void scene_manager::add_camera_component(entity entity, const CameraComponent& camera) {
  entity_component& object = get_scene_object_by_entity(entity).value().get();
  object.camera = resource_manager_->get_database().add_camera(camera);
}

void scene_manager::add_light_component(entity entity, const LightComponent& light) {
  entity_component& object = get_scene_object_by_entity(entity).value().get();
  object.camera = resource_manager_->get_database().add_light(light);
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


void scene_manager::update_scene(draw_context& draw_context) {
  constexpr glm::mat4 identity = glm::mat4(1.0f);
  for (const auto& root_node_entity : root_.children) {
    traverse_scene(root_node_entity, identity, draw_context);
  }
}

void scene_manager::traverse_scene(const entity entity, const glm::mat4& parent_transform,
                                      draw_context& draw_context) {
  assert(entity != invalid_entity);
  const auto& object = get_scene_object_by_entity(entity).value().get();
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
        draw_context.transparent_surfaces.push_back(def);
      } else {
        draw_context.opaque_surfaces.push_back(def);
      }
    }
  }

  // Process child nodes recursively, passing the current world transform
  for (const auto& childEntity : object.children) {
    traverse_scene(childEntity, world_transform, draw_context);
  }
}

void scene_manager::init_default_data() const {
  auto& default_material = resource_manager_->get_database().default_material_;

  uint32_t white = 0xFFFFFFFF;                       // White color for color and occlusion
  uint32_t default_metallic_roughness = 0xFF00FF00;  // Green color representing metallic-roughness
  uint32_t flat_normal = 0xFFFF8080;                 // Flat normal
  uint32_t black = 0xFF000000;                       // Black color for emissive

  default_material.color_image = resource_manager_->create_image(
      (void*)&white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
  resource_manager_->get_database().add_image(default_material.color_image);

  default_material.metallic_roughness_image
      = resource_manager_->create_image((void*)&default_metallic_roughness, VkExtent3D{1, 1, 1},
                                        VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
  resource_manager_->get_database().add_image(default_material.metallic_roughness_image);

  default_material.normal_image
      = resource_manager_->create_image((void*)&flat_normal, VkExtent3D{1, 1, 1},
                                        VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
  resource_manager_->get_database().add_image(default_material.normal_image);

  default_material.emissive_image = resource_manager_->create_image(
      (void*)&black, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
  resource_manager_->get_database().add_image(default_material.emissive_image);

  default_material.occlusion_image = resource_manager_->create_image(
      (void*)&white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
  resource_manager_->get_database().add_image(default_material.occlusion_image);

  // checkerboard image for error textures and testing
  uint32_t magenta = 0xFFFF00FF;
  constexpr size_t checkerboard_size = 256;
  std::array<uint32_t, checkerboard_size> pixels;  // for 16x16 checkerboard texture
  for (int x = 0; x < 16; x++) {
    for (int y = 0; y < 16; y++) {
      pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
    }
  }
  default_material.error_checkerboard_image = resource_manager_->create_image(
      pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
  resource_manager_->get_database().add_image(default_material.error_checkerboard_image);

  VkSamplerCreateInfo sampler = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

  sampler.magFilter = VK_FILTER_NEAREST;
  sampler.minFilter = VK_FILTER_NEAREST;

  vkCreateSampler(gpu_.device, &sampler, nullptr, &default_material.default_sampler_nearest);
  resource_manager_->get_database().add_sampler(default_material.default_sampler_nearest);

  sampler.magFilter = VK_FILTER_LINEAR;
  sampler.minFilter = VK_FILTER_LINEAR;
  vkCreateSampler(gpu_.device, &sampler, nullptr, &default_material.default_sampler_linear);
  resource_manager_->get_database().add_sampler(default_material.default_sampler_linear);

  pbr_material material{};
  material.constants.albedo_factor.x = 1.f;
  material.constants.albedo_factor.y = 1.f;
  material.constants.albedo_factor.z = 1.f;
  material.constants.albedo_factor.w = 1.f;

  material.constants.metal_rough_factor.x = 0.f;
  material.constants.metal_rough_factor.y = 0.f;
  // write material parameters to buffer

  // default the material textures
  material.resources.albedo_image = default_material.color_image;
  material.resources.albedo_sampler = default_material.default_sampler_linear;
  material.resources.metal_rough_image = default_material.metallic_roughness_image;
  material.resources.metal_rough_sampler = default_material.default_sampler_linear;
  material.resources.normal_image = default_material.normal_image;
  material.resources.normal_sampler = default_material.default_sampler_linear;
  material.resources.emissive_image = default_material.emissive_image;
  material.resources.emissive_sampler = default_material.default_sampler_linear;
  material.resources.occlusion_image = default_material.occlusion_image;
  material.resources.occlusion_sampler = default_material.default_sampler_nearest;

  // build material
  create_material(
      material, "default_material");
}
