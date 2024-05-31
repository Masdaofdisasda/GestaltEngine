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
      void init(const std::shared_ptr<IGpu>& gpu,
                const std::shared_ptr<IResourceManager>& resource_manager,
                const std::unique_ptr<IDescriptorUtilFactory>& descriptor_util_factory,
                const std::shared_ptr<Repository>& repository) {
        gpu_ = gpu;
        resource_manager_ = resource_manager;
        descriptor_layout_builder_ = descriptor_util_factory->create_descriptor_layout_builder();
        writer_ = descriptor_util_factory->create_descriptor_writer();
        repository_ = repository;

        prepare();
      }
      virtual ~SceneSystem() = default;

      virtual void update() = 0;
      virtual void cleanup() = 0;

    protected:
      virtual void prepare() = 0;

      std::shared_ptr<IGpu> gpu_;
      std::shared_ptr<IResourceManager> resource_manager_;
      std::unique_ptr<IDescriptorLayoutBuilder> descriptor_layout_builder_;
      std::unique_ptr<IDescriptorWriter> writer_;
      std::shared_ptr<Repository> repository_;
    };

    class MaterialSystem final : public SceneSystem {
    public:
      void create_defaults();
      void prepare() override;
      void update() override;
      void cleanup() override;
      void write_material(PbrMaterial& material, uint32_t material_id);
    };

    class LightSystem final : public SceneSystem {

      glm::mat4 calculate_sun_view_proj(glm::vec3 direction) const;
      void update_directional_lights(std::unordered_map<entity, LightComponent>& lights);
      void update_point_lights(std::unordered_map<entity, LightComponent>& lights);

    public:
      void prepare() override;
      void update() override;
      void cleanup() override;
    };

    class CameraSystem final : public SceneSystem {
      std::unique_ptr<Camera> active_camera_;
      std::unique_ptr<FreeFlyCamera> free_fly_camera_; // move to components
      float32 aspect_{1.f};

    public:
      void prepare() override;
      void update_cameras(float delta_time, const Movement& movement, float aspect);
      void update() override;
      void cleanup() override;
    };

    class TransformSystem final : public SceneSystem {
      void mark_children_dirty(entity entity);
      void mark_as_dirty(entity entity);
      void update_aabb(entity entity, const glm::mat4& parent_transform);
      void mark_parent_dirty(entity entity);

    public:
      static glm::mat4 get_model_matrix(const TransformComponent& transform);

      void prepare() override;
      void update() override;
      void cleanup() override;
    };

    class RenderSystem final : public SceneSystem {
      size_t meshes_ = 0;
      void traverse_scene(entity entity, const glm::mat4& parent_transform);

    public:
      void prepare() override;
      void upload_mesh();
      void update() override;
      void cleanup() override;
    };

}  // namespace gestalt