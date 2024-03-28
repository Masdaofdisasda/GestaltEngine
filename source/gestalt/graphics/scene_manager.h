#pragma once

#include <filesystem>

#include "scene_components.h"

#include "resource_manager.h"
#include "scene_systems.h"

class asset_loader;

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


class asset_loader {
  vk_gpu gpu_ = {};
  std::shared_ptr<resource_manager> resource_manager_;
  std::shared_ptr<component_archetype_factory> component_factory_;

  static VkFilter extract_filter(fastgltf::Filter filter);
  VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter);
  std::optional<fastgltf::Asset> parse_gltf(const std::filesystem::path& file_path);
  void import_samplers(fastgltf::Asset& gltf);
  std::optional<AllocatedImage> load_image(fastgltf::Asset& asset, fastgltf::Image& image) const;
  void import_textures(fastgltf::Asset& gltf) const;
  size_t create_material(pbr_material& config, const std::string& name) const;
  std::tuple<AllocatedImage, VkSampler> get_textures(const fastgltf::Asset& gltf,
                                                     const size_t& texture_index,
                                                     const size_t& image_offset,
                                                     const size_t& sampler_offset) const;
  void import_albedo(const fastgltf::Asset& gltf, const size_t& sampler_offset,
                     const size_t& image_offset, const fastgltf::Material& mat,
                     pbr_material& pbr_config) const;
  void import_metallic_roughness(const fastgltf::Asset& gltf, const size_t& sampler_offset,
                                 const size_t& image_offset, const fastgltf::Material& mat,
                                 pbr_material& pbr_config) const;
  void import_normal(const fastgltf::Asset& gltf, const size_t& sampler_offset,
                     const size_t& image_offset, const fastgltf::Material& mat,
                     pbr_material& pbr_config) const;
  void import_emissive(const fastgltf::Asset& gltf, const size_t& sampler_offset,
                       const size_t& image_offset, const fastgltf::Material& mat,
                       pbr_material& pbr_config) const;
  void import_occlusion(const fastgltf::Asset& gltf, const size_t& sampler_offset,
                        const size_t& image_offset, const fastgltf::Material& mat,
                        pbr_material& pbr_config) const;
  void import_material(fastgltf::Asset& gltf, size_t& sampler_offset, size_t& image_offset,
                       fastgltf::Material& mat) const;
  void import_materials(fastgltf::Asset& gltf, size_t& sampler_offset, size_t& image_offset) const;
  static void optimize_mesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
  void simplify_mesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
  size_t create_surface(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
  size_t create_mesh(std::vector<size_t> surfaces, const std::string& name) const;
  void add_material_component(size_t surface, size_t material) const;
  void import_meshes(fastgltf::Asset& gltf, size_t material_offset);

public:
  void init(const vk_gpu& gpu, const std::shared_ptr<resource_manager>& resource_manager,
            const std::shared_ptr<component_archetype_factory>& component_factory);
  void import_lights(const fastgltf::Asset& gltf);
  std::vector<fastgltf::Node> load_scene_from_gltf(const std::string& file_path);
};

/**
 * @brief Class responsible for managing scenes, entities, and their components.
 */
class scene_manager {
  vk_gpu gpu_ = {};
  std::shared_ptr<resource_manager> resource_manager_;
  std::unique_ptr<asset_loader> asset_loader_ = std::make_unique<asset_loader>();
  std::shared_ptr<component_archetype_factory> component_factory_
      = std::make_shared<component_archetype_factory>();

  std::unique_ptr<scene_system> light_system_;
  std::unique_ptr<scene_system> transform_system_;
  std::unique_ptr<scene_system> render_system_;


  void build_scene_graph(const std::vector<fastgltf::Node>& nodes, const size_t& mesh_offset);
  void create_entities(std::vector<fastgltf::Node> nodes, const size_t& mesh_offset);
  void build_hierarchy(std::vector<fastgltf::Node> nodes, const size_t& node_offset);
  void link_orphans_to_root();
  const entity root_entity_ = 0;

  void load_scene(const std::string& path);
  std::string scene_path_ = "";

public:
  void init(const vk_gpu& gpu, const std::shared_ptr<resource_manager>& resource_manager);
  void cleanup();

  void update_scene();

  void request_scene(const std::string& path);
  component_archetype_factory& get_component_factory() const { return *component_factory_; }
  node_component& get_root_node();
  uint32_t get_root_entity() { return root_entity_; }
  void add_to_root(entity entity, node_component& node);
};