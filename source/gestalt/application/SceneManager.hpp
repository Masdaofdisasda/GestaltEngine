#pragma once

#include <filesystem>
#include <vector>

#include "common.hpp"

#include "Components.hpp"
#include "SceneSystem.hpp"

namespace gestalt::application {

    struct Vertex;
    class AssetLoader;

    class ComponentFactory : public NonCopyable<ComponentFactory> {
      std::shared_ptr<IResourceManager> resource_manager_;
      std::shared_ptr<Repository> repository_;

      entity next_entity_id_ = 0;

    public:
      void init(const std::shared_ptr<IResourceManager>& resource_manager,
                const std::shared_ptr<Repository>& repository);

      entity create_entity();
      std::pair<entity, std::reference_wrapper<NodeComponent>> create_entity_node(
          std::string node_name = "");
      void add_mesh_component(entity entity, size_t mesh_index);
      void add_camera_component(entity entity, const CameraComponent& camera);

      entity create_directional_light(const glm::vec3& color, const float intensity,
                                      const glm::vec3& direction, entity parent = 0);
      entity create_spot_light(const glm::vec3& color, const float intensity,
                               const glm::vec3& direction, const glm::vec3& position,
                               const float innerCone, const float outerCone, entity parent = 0);
      entity create_point_light(const glm::vec3& color, const float intensity,
                                const glm::vec3& position, entity parent = 0);

      void link_entity_to_parent(entity child, entity parent);
      void update_transform_component(unsigned entity, const glm::vec3& position,
                                      const glm::quat& rotation = glm::quat(0.f, 0.f, 0.f, 0.f),
                                      const float& scale = 1.f);
      void create_transform_component(unsigned entity, const glm::vec3& position,
                                      const glm::quat& rotation = glm::quat(0.f, 0.f, 0.f, 0.f),
                                      const float& scale = 1.f) const;
    };

    class AssetLoader : public NonCopyable<AssetLoader> {
      std::shared_ptr<IResourceManager> resource_manager_;
      std::shared_ptr<Repository> repository_;
      std::shared_ptr<ComponentFactory> component_factory_;

      static VkFilter extract_filter(fastgltf::Filter filter);
      VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter);
      std::optional<fastgltf::Asset> parse_gltf(const std::filesystem::path& file_path);
      void import_samplers(fastgltf::Asset& gltf);
      std::optional<TextureHandle> load_image(fastgltf::Asset& asset,
                                              fastgltf::Image& image) const;
      void import_textures(fastgltf::Asset& gltf) const;
      size_t create_material(const PbrMaterial& config, const std::string& name) const;
      std::tuple<TextureHandle, VkSampler> get_textures(const fastgltf::Asset& gltf,
                                                        const size_t& texture_index,
                                                        const size_t& image_offset,
                                                        const size_t& sampler_offset) const;
      void import_albedo(const fastgltf::Asset& gltf, const size_t& sampler_offset,
                         const size_t& image_offset, const fastgltf::Material& mat,
                         PbrMaterial& pbr_config) const;
      void import_metallic_roughness(const fastgltf::Asset& gltf, const size_t& sampler_offset,
                                     const size_t& image_offset, const fastgltf::Material& mat,
                                     PbrMaterial& pbr_config) const;
      void import_normal(const fastgltf::Asset& gltf, const size_t& sampler_offset,
                         const size_t& image_offset, const fastgltf::Material& mat,
                         PbrMaterial& pbr_config) const;
      void import_emissive(const fastgltf::Asset& gltf, const size_t& sampler_offset,
                           const size_t& image_offset, const fastgltf::Material& mat,
                           PbrMaterial& pbr_config) const;
      void import_occlusion(const fastgltf::Asset& gltf, const size_t& sampler_offset,
                            const size_t& image_offset, const fastgltf::Material& mat,
                            PbrMaterial& pbr_config) const;
      void import_material(fastgltf::Asset& gltf, size_t& sampler_offset, size_t& image_offset,
                           fastgltf::Material& mat) const;
      void import_materials(fastgltf::Asset& gltf, size_t& sampler_offset,
                            size_t& image_offset) const;
      static void optimize_mesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
      void simplify_mesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
      std::vector<Meshlet> generate_meshlets(std::vector<GpuVertexPosition>& vertices,
                                             std::vector<uint32_t>& indices);
      MeshSurface create_surface(std::vector<GpuVertexPosition>& vertex_positions,
                                 std::vector<GpuVertexData>& vertex_data,
                                 std::vector<uint32_t>& indices, std::vector<Meshlet>&& meshlets);
      size_t create_mesh(std::vector<MeshSurface> surfaces, const std::string& name) const;
      void add_material_component(MeshSurface& surface, const size_t material) const;
      void import_meshes(fastgltf::Asset& gltf, size_t material_offset);

    public:
      void init(const std::shared_ptr<IResourceManager>& resource_manager,
                const std::shared_ptr<ComponentFactory>& component_factory,
                const std::shared_ptr<Repository>& repository);
      void import_lights(const fastgltf::Asset& gltf);
      std::vector<fastgltf::Node> load_scene_from_gltf(const std::string& file_path);
    };

    /**
     * @brief Class responsible for managing scenes, entities, and their components.
     */
    class SceneManager : public NonCopyable<SceneManager> {
      std::shared_ptr<IGpu> gpu_;
      std::shared_ptr<IResourceManager> resource_manager_;
      std::shared_ptr<Repository> repository_;

      std::unique_ptr<AssetLoader> asset_loader_ = std::make_unique<AssetLoader>();
      std::shared_ptr<ComponentFactory> component_factory_
          = std::make_shared<ComponentFactory>();

      std::unique_ptr<MaterialSystem> material_system_;
      std::unique_ptr<SceneSystem> light_system_;
      std::unique_ptr<CameraSystem> camera_system_;
      std::unique_ptr<SceneSystem> transform_system_;
      std::unique_ptr<SceneSystem> render_system_;

      void build_scene_graph(const std::vector<fastgltf::Node>& nodes, const size_t& mesh_offset);
      void create_entities(std::vector<fastgltf::Node> nodes, const size_t& mesh_offset);
      void build_hierarchy(std::vector<fastgltf::Node> nodes, const size_t& node_offset);
      void link_orphans_to_root();
      entity root_entity_ = 0;

      void load_scene(const std::string& path);
      std::string scene_path_;

    public:
      void init(const std::shared_ptr<IGpu>& gpu,
                const std::shared_ptr<IResourceManager>& resource_manager,
                const std::unique_ptr<IDescriptorUtilFactory>& descriptor_util_factory,
                const std::shared_ptr<Repository>& repository);
      void cleanup() const;

      void update_scene(float delta_time, const Movement& movement, float aspect);

      void request_scene(const std::string& path);
      ComponentFactory& get_component_factory() const { return *component_factory_; }
      NodeComponent& get_root_node();
      uint32 get_root_entity() { return root_entity_; }
      void add_to_root(entity entity, NodeComponent& node);
    };
}  // namespace gestalt::application