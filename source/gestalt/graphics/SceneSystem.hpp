#pragma once

#include "Components.hpp"
#include "Camera.hpp"
#include "ResourceManager.hpp"
#include "Gpu.hpp"

namespace gestalt::application {
    struct Movement;

    class SceneSystem {
    public:
      void init(const graphics::Gpu& gpu, const std::shared_ptr<graphics::ResourceManager>& resource_manager,
                const std::shared_ptr<Repository>& repository) {
        gpu_ = gpu;
        resource_manager_ = resource_manager;
        repository_ = repository;

        prepare();
      }
      virtual ~SceneSystem() = default;

      virtual void update() = 0;
      virtual void cleanup() = 0;

    protected:
      virtual void prepare() = 0;

      graphics::Gpu gpu_ = {};
      std::shared_ptr<graphics::ResourceManager> resource_manager_;
      std::shared_ptr<Repository> repository_;
    };

    class MaterialSystem final : public SceneSystem {
      graphics::descriptor_writer writer_;
    public:
      void create_defaults();
      void prepare() override;
      void update() override;
      void cleanup() override;
      void write_material(PbrMaterial& material, uint32_t material_id);
    };

    class LightSystem final : public SceneSystem {
      graphics::descriptor_writer writer_;


      glm::mat4 calculate_sun_view_proj(const glm::vec3 direction) const;
      void update_directional_lights(std::unordered_map<entity, LightComponent>& lights);
      void update_point_lights(std::unordered_map<entity, LightComponent>& lights);

    public:
      void prepare() override;
      void update() override;
      void cleanup() override;
    };

    class CameraSystem final : public SceneSystem {
      graphics::descriptor_writer writer_;
      std::unique_ptr<Camera> active_camera_;
      std::unique_ptr<FreeFlyCamera> free_fly_camera_; // move to components
      float32 aspect_{1.f};

    public:
      void prepare() override;
      void update_cameras(const float delta_time, const Movement& movement, float aspect);
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
      void traverse_scene(const entity entity, const glm::mat4& parent_transform);

    public:
      void prepare() override;
      void update() override;
      void cleanup() override {}
    };

    class HierarchySystem final : public SceneSystem {
      void build_scene_graph(const std::vector<fastgltf::Node>& nodes, const size_t& mesh_offset);
      void create_entities(std::vector<fastgltf::Node> nodes, const size_t& mesh_offset);
      void build_hierarchy(std::vector<fastgltf::Node> nodes, const size_t& node_offset);
      void link_orphans_to_root();

    public:
      void prepare() override;
      void update() override;
      void cleanup() override;
    };
}  // namespace gestalt