#pragma once

#include <filesystem>
#include <vector>

#include "common.hpp"

#include "SceneSystem.hpp"
#include "Components/PhysicsComponent.hpp"
#include "Material/PbrMaterial.hpp"

namespace fastgltf {
  struct Material;
  struct Image;
  class Asset;
}

namespace gestalt::foundation {
  class IDescriptorLayoutBuilder;
  class IGpu;
  class IResourceManager;
}

namespace gestalt::application {

    struct Vertex;

    class ComponentFactory : public NonCopyable<ComponentFactory> {
      IResourceManager* resource_manager_ = nullptr;
      Repository* repository_ = nullptr;

      Entity next_entity_id_{0};

      Entity next_entity();
      void create_transform_component(unsigned entity, const glm::vec3& position = glm::vec3(0.f),
                                      const glm::quat& rotation = glm::quat(1.f, 0.f, 0.f, 0.f),
                                      const float& scale = 1.f) const;

    public:
      void init(IResourceManager* resource_manager, Repository* repository);

      std::pair<Entity, std::reference_wrapper<NodeComponent>> create_entity(
          std::string node_name = "", const glm::vec3& position = glm::vec3(0.f),
          const glm::quat& rotation = glm::quat(1.f, 0.f, 0.f, 0.f), const float& scale = 1.f);
      void add_mesh_component(Entity entity, size_t mesh_index);
      void create_mesh(std::vector<MeshSurface> surfaces, const std::string& name) const;
      void create_physics_component(Entity entity, BodyType body_type,
                                    const BoxCollider& collider) const;
      void create_physics_component(Entity entity, BodyType body_type,
                                    const SphereCollider& collider) const;
      void create_physics_component(Entity entity, BodyType body_type,
                                    const CapsuleCollider& collider) const;

      void create_animation_component(const Entity entity,
                                      const std::vector<Keyframe<glm::vec3>>& translation_keyframes,
                                      const std::vector<Keyframe<glm::quat>>& rotation_keyframes,
                                      const std::vector<Keyframe<glm::vec3>>& scale_keyframes) const;

      Entity create_directional_light(const glm::vec3& color, float intensity,
                                      const glm::vec3& direction, Entity parent = 0);
      Entity create_spot_light(const glm::vec3& color, float intensity,
                               const glm::vec3& direction, const glm::vec3& position,
                               float innerCone, float outerCone, Entity parent = 0);
      Entity create_point_light(const glm::vec3& color, float intensity, const glm::vec3& position,
                                float32 range, Entity parent = 0);
      Entity add_free_fly_camera(const glm::vec3& position, const glm::vec3& direction,
                                 const glm::vec3& up, Entity entity,
                                 ProjectionData projection_data
                                 = PerspectiveProjectionData(70.f, 1.0f, 0.1f, 10000.f)) const;

      Entity add_animation_camera(const glm::vec3& position, const glm::quat& orientation,
                                  Entity entity,
                                  ProjectionData projection_data
                                  = PerspectiveProjectionData(70.f, 1.0f, 0.1f, 10000.f)) const;

      Entity add_orbit_camera(const glm::vec3& target, Entity entity,
                              ProjectionData projection_data
                              = PerspectiveProjectionData(70.f, 1.0f, 0.1f, 10000.f)) const;

      Entity add_first_person_camera(const glm::vec3& position, Entity entity,
                                     ProjectionData projection_data
                                     = PerspectiveProjectionData(70.f, 1.0f, 0.1f, 10000.f)) const;

      void link_entity_to_parent(Entity child, Entity parent);
    };

    class AssetLoader : public NonCopyable<AssetLoader> {
      IResourceManager* resource_manager_ = nullptr;
      Repository* repository_ = nullptr;
      ComponentFactory* component_factory_ = nullptr;

      std::optional<fastgltf::Asset> parse_gltf(const std::filesystem::path& file_path);
      std::optional<TextureHandle> load_image(fastgltf::Asset& asset,
                                              fastgltf::Image& image) const;
      void import_textures(fastgltf::Asset& gltf) const;
      size_t create_material(const PbrMaterial& config, const std::string& name) const;
      TextureHandle get_textures(const fastgltf::Asset& gltf,
                                                        const size_t& texture_index,
                                                        const size_t& image_offset) const;
      void import_albedo(const fastgltf::Asset& gltf,
                         const size_t& image_offset, const fastgltf::Material& mat,
                         PbrMaterial& pbr_config) const;
      void import_metallic_roughness(const fastgltf::Asset& gltf,
                                     const size_t& image_offset, const fastgltf::Material& mat,
                                     PbrMaterial& pbr_config) const;
      void import_normal(const fastgltf::Asset& gltf, 
                         const size_t& image_offset, const fastgltf::Material& mat,
                         PbrMaterial& pbr_config) const;
      void import_emissive(const fastgltf::Asset& gltf, 
                           const size_t& image_offset, const fastgltf::Material& mat,
                           PbrMaterial& pbr_config) const;
      void import_occlusion(const fastgltf::Asset& gltf, 
                            const size_t& image_offset, const fastgltf::Material& mat,
                            PbrMaterial& pbr_config) const;
      void import_material(fastgltf::Asset& gltf, size_t& image_offset,
                           fastgltf::Material& mat) const;
      void import_materials(fastgltf::Asset& gltf,
                            size_t& image_offset) const;
      void import_meshes(fastgltf::Asset& gltf, size_t material_offset) const;

    public:
      void init(IResourceManager* resource_manager,
                ComponentFactory* component_factory,
                Repository* repository);
      void import_lights(const fastgltf::Asset& gltf);
      void import_nodes(fastgltf::Asset& gltf) const;
      void load_scene_from_gltf(const std::string& file_path);
      void import_animations(const fastgltf::Asset& gltf, const size_t node_offset);
    };

    /**
     * @brief Class responsible for managing scenes, entities, and their components.
     */
    class EntityManager : public NonCopyable<EntityManager> {
      IGpu* gpu_ = nullptr;
      IResourceManager* resource_manager_ = nullptr;
      Repository* repository_ = nullptr;

      std::unique_ptr<AssetLoader> asset_loader_ = std::make_unique<AssetLoader>();
      std::unique_ptr<ComponentFactory> component_factory_
          = std::make_unique<ComponentFactory>();
      std::unique_ptr<NotificationManager> notification_manager_ = std::make_unique<NotificationManager>();
      std::unique_ptr<MaterialSystem> material_system_;
      std::unique_ptr<SceneSystem> light_system_;
      std::unique_ptr<CameraSystem> camera_system_;
      std::unique_ptr<SceneSystem> transform_system_;
      std::unique_ptr<AnimationSystem> animation_system_;
      std::unique_ptr<SceneSystem> mesh_system_;
      std::unique_ptr<PhysicSystem> physics_system_;

      Entity root_entity_ = 0;

      void load_scene(const std::string& path);
      std::string scene_path_;

    public:
      void init(IGpu* gpu,
                IResourceManager* resource_manager,
                IDescriptorLayoutBuilder* builder,
                Repository* repository, FrameProvider* frame);
      void cleanup() const;

      void update_scene(float delta_time, const UserInput& movement, float aspect);

      void request_scene(const std::string& path);
      ComponentFactory& get_component_factory() const { return *component_factory_; }
      NodeComponent& get_root_node();
      uint32 get_root_entity() { return root_entity_; }
      void add_to_root(Entity entity, NodeComponent& node);
    };
}  // namespace gestalt::application