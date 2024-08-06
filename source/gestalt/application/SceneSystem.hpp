#pragma once

#include <unordered_map>

#include "Components.hpp"
#include "Camera.hpp"
#include "GpuTypes.hpp"
#include "Repository.hpp"

#include "fastgltf/types.hpp"

namespace gestalt::application {
    struct Movement;

    class SceneSystem {
    public:
      void init(IGpu* gpu,
                IResourceManager* resource_manager,
                IDescriptorLayoutBuilder* builder,
                Repository* repository) {
        gpu_ = gpu;
        resource_manager_ = resource_manager;
        descriptor_layout_builder_ = builder;
        repository_ = repository;

        prepare();
      }
      virtual ~SceneSystem() = default;

      virtual void update() = 0;
      virtual void cleanup() = 0;

    protected:
      virtual void prepare() = 0;

      IGpu* gpu_ = nullptr;
      IResourceManager* resource_manager_ = nullptr;
      IDescriptorLayoutBuilder* descriptor_layout_builder_ = nullptr;
      Repository* repository_ = nullptr;
    };

    class MaterialSystem final : public SceneSystem, public NonCopyable<MaterialSystem> {
      std::array<VkDescriptorImageInfo, getMaxTextures()> image_infos_ = {};

      void create_buffers();
      void create_default_material();
      void fill_uniform_buffer();
      void fill_images_buffer();
      void write_material(PbrMaterial& material, uint32 material_id);

    public:
      void prepare() override;
      void update() override;
      void cleanup() override;
    };

    class LightSystem final : public SceneSystem, public NonCopyable<LightSystem> {

      void create_buffers();
      void fill_buffers();

      glm::mat4 calculate_sun_view_proj(glm::vec3 direction) const;
      void update_directional_lights(std::unordered_map<Entity, LightComponent>& lights);
      void update_point_lights(std::unordered_map<Entity, LightComponent>& lights);

    public:
      void prepare() override;
      void update() override;
      void cleanup() override;
    };

    class CameraSystem final : public SceneSystem, public NonCopyable<CameraSystem> {
      std::unique_ptr<Camera> active_camera_;
      std::unique_ptr<FreeFlyCamera> free_fly_camera_; // move to components
      float32 aspect_{1.f};
      float32 near_plane_{0.1f};
      float32 far_plane_{10000.f};
      float32 fov_{70.f};

    public:
      void prepare() override;
      void update_cameras(float delta_time, const Movement& movement, float aspect);
      void update() override;
      void cleanup() override;
    };

    class TransformSystem final : public SceneSystem, public NonCopyable<TransformSystem> {
      void mark_children_dirty(Entity entity);
      void mark_as_dirty(Entity entity);
      void update_aabb(Entity entity, const glm::mat4& parent_transform);
      void mark_parent_dirty(Entity entity);

    public:
      static glm::mat4 get_model_matrix(const TransformComponent& transform);

      void prepare() override;
      void update() override;
      void cleanup() override;
    };

    class MeshSystem final : public SceneSystem, public NonCopyable<MeshSystem> {
      size_t meshes_ = 0;
      void traverse_scene(Entity entity, const glm::mat4& parent_transform);
      void upload_mesh();

      void create_buffers();
      void fill_descriptors();

    public:
      void prepare() override;
      void update() override;
      void cleanup() override;
    };

}  // namespace gestalt