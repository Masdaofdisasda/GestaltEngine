
#include <scene_manager.h>

#include "vk_types.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>

#include <glm/gtx/matrix_decompose.hpp>

void scene_manager::init(const vk_gpu& gpu,const std::shared_ptr <resource_manager>& resource_manager) {
  gpu_ = gpu;
  resource_manager_ = resource_manager;
  deletion_service_.init(gpu.device, gpu.allocator);
  asset_loader_->init(gpu_, resource_manager_);

  init_default_data();
  //load_scene_from_gltf(R"(..\..\assets\Models\MetalRoughSpheres\glTF-Binary\MetalRoughSpheres.glb)");
  auto nodes = asset_loader_->load_scene_from_gltf(R"(..\..\assets\sponza_pestana.glb)");
  //load_scene_from_gltf(R"(..\..\assets\awesome-3d-meshes\McGuire\Amazon Lumberyard Bistro\gltf\Bistro.glb)");
  //load_scene_from_gltf(R"(..\..\assets\structure.glb)");

  build_scene_graph(nodes);

  load_environment_map(R"(..\..\assets\san_giuseppe_bridge_4k.hdr)"); 
}

void scene_manager::create_entities(std::vector<fastgltf::Node> nodes) {
  for (fastgltf::Node& node : nodes) {
    entity_component& new_node = create_entity();

    if (node.meshIndex.has_value()) {
      add_mesh_component(new_node.entity, *node.meshIndex);
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

void scene_manager::build_hierarchy(std::vector<fastgltf::Node> nodes) {
  for (int i = 0; i < nodes.size(); i++) {
    fastgltf::Node& node = nodes[i];
    auto& scene_object = get_scene_object_by_entity(i).value().get();

    for (auto& c : node.children) {
      auto child = get_scene_object_by_entity(c).value().get();
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

void scene_manager::build_scene_graph(const std::vector<fastgltf::Node>& nodes) {
  create_entities(nodes);
  build_hierarchy(nodes);
  link_orphans_to_root();
}

void scene_manager::cleanup() {
  for (auto img : resource_manager_->get_database().get_images()) {
    resource_manager_->destroy_image(img);
  }

  deletion_service_.flush();
}

void scene_manager::load_environment_map(const std::string& file_path) {
  resource_manager_->load_and_process_cubemap(file_path);
}

size_t scene_manager::create_material(const gltf_material& material, const pbr_config& config,
                                     const std::string& name) const {
  const size_t material_id = resource_manager_->get_database().get_materials_size();
  fmt::print("creating material {}, mat_id {}\n", name, material_id);

  const std::string key = name.empty() ? "material_" + std::to_string(material_id) : name;
  resource_manager_->write_material(material, material_id);

  resource_manager_->get_database().add_material(material_component{.name = key, .config = config});

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

void scene_manager::init_default_data() {
  auto& default_material = resource_manager_->get_database().default_material_;

  uint32_t white = 0xFFFFFFFF;                       // White color for color and occlusion
  uint32_t default_metallic_roughness = 0xFF00FF00;  // Green color representing metallic-roughness
  uint32_t flat_normal = 0xFFFF8080;                 // Flat normal
  uint32_t black = 0xFF000000;                       // Black color for emissive

  default_material.color_image = resource_manager_->create_image(
      (void*)&white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

  default_material.metallic_roughness_image
      = resource_manager_->create_image((void*)&default_metallic_roughness, VkExtent3D{1, 1, 1},
                                       VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

  default_material.normal_image
      = resource_manager_->create_image((void*)&flat_normal, VkExtent3D{1, 1, 1},
                                       VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

  default_material.emissive_image = resource_manager_->create_image(
      (void*)&black, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

  default_material.occlusion_image = resource_manager_->create_image(
      (void*)&white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

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

  VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

  sampl.magFilter = VK_FILTER_NEAREST;
  sampl.minFilter = VK_FILTER_NEAREST;

  vkCreateSampler(gpu_.device, &sampl, nullptr, &default_material.default_sampler_nearest);

  sampl.magFilter = VK_FILTER_LINEAR;
  sampl.minFilter = VK_FILTER_LINEAR;
  vkCreateSampler(gpu_.device, &sampl, nullptr, &default_material.default_sampler_linear);

  gltf_material::MaterialConstants constants;
  constants.colorFactors.x = 1.f;
  constants.colorFactors.y = 1.f;
  constants.colorFactors.z = 1.f;
  constants.colorFactors.w = 1.f;

  constants.metal_rough_factors.x = 0.f;
  constants.metal_rough_factors.y = 0.f;
  // write material parameters to buffer

  gltf_material::MaterialResources material_resources;
  // default the material textures
  material_resources.colorImage = default_material.color_image;
  material_resources.colorSampler = default_material.default_sampler_linear;
  material_resources.metalRoughImage = default_material.metallic_roughness_image;
  material_resources.metalRoughSampler = default_material.default_sampler_linear;
  material_resources.normalImage = default_material.normal_image;
  material_resources.normalSampler = default_material.default_sampler_linear;
  material_resources.emissiveImage = default_material.emissive_image;
  material_resources.emissiveSampler = default_material.default_sampler_linear;
  material_resources.occlusionImage = default_material.occlusion_image;
  material_resources.occlusionSampler = default_material.default_sampler_nearest;

  pbr_config config{};
  // build material
  create_material(
      {
          constants,
          material_resources,
      },
      config, "default_material");

  deletion_service_.push_function(
      [this, default_material]() { resource_manager_->destroy_image(default_material.color_image); });
  deletion_service_.push_function(
      [this, default_material]() { resource_manager_->destroy_image(default_material.metallic_roughness_image); });
  deletion_service_.push_function(
      [this, default_material]() { resource_manager_->destroy_image(default_material.normal_image); });
  deletion_service_.push_function(
      [this, default_material]() { resource_manager_->destroy_image(default_material.emissive_image); });
  deletion_service_.push_function(
      [this, default_material]() { resource_manager_->destroy_image(default_material.occlusion_image); });
  deletion_service_.push_function(
      [this, default_material]() { resource_manager_->destroy_image(default_material.error_checkerboard_image); });
  deletion_service_.push(default_material.default_sampler_nearest);
  deletion_service_.push(default_material.default_sampler_linear);
}