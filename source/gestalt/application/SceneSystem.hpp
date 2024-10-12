#pragma once

#include <functional>
#include <queue>
#include <unordered_map>

#include "Camera.hpp"
#include "PhysicEngine.hpp"
#include "Repository.hpp"
#include "Animation/AnimationChannel.hpp"

namespace JPH {
  class Body;
  class BodyID;
  class JobSystemThreadPool;
  class TempAllocatorImpl;
  class BodyInterface;
  class PhysicsSystem;
}

namespace gestalt::foundation {
  class IResourceManager;
  class IDescriptorLayoutBuilder;
  class IGpu;
  struct FrameProvider;
  struct PbrMaterial;
}

namespace gestalt::application {
  class MyContactListener;
  class MyBodyActivationListener;
  class ObjectLayerPairFilterImpl;
  class ObjectVsBroadPhaseLayerFilterImpl;
  class BPLayerInterfaceImpl;

  enum class ChangeType {
      ComponentUpdated,
    };
    struct ChangeEvent {
      ChangeType type;
      Entity entityId;
    };


  // TODO rethink this
class NotificationManager {
    public:
      using Callback = std::function<void(const ChangeEvent&)>;

      // Subscribe to a specific change type
      void subscribe(ChangeType changeType, Callback callback) {
        subscribers_[changeType].push_back(callback);
      }

      // Notify all subscribers of a specific change event
      void notify(const ChangeEvent& event) {
        for (const auto& callback : subscribers_[event.type]) {
          callback(event);
        }
      }

      // Record a change event for later processing
      void recordChange(ChangeType changeType, Entity entityId) {
        changeQueue_.push(ChangeEvent{changeType, entityId});
      }

      // Process all recorded change events
      void processChanges() {
        while (!changeQueue_.empty()) {
          const auto& event = changeQueue_.front();
          notify(event);
          changeQueue_.pop();
        }
      }

    private:
      std::unordered_map<ChangeType, std::vector<Callback>> subscribers_;
      std::queue<ChangeEvent> changeQueue_;
    };

    class SceneSystem {
    public:
      void init(IGpu* gpu,
                IResourceManager* resource_manager,
                IDescriptorLayoutBuilder* builder,
                Repository* repository, NotificationManager* notification_manager,
                FrameProvider* frame) {
        gpu_ = gpu;
        resource_manager_ = resource_manager;
        descriptor_layout_builder_ = builder;
        repository_ = repository;
        notification_manager_ = notification_manager;
        frame_ = frame; 

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
      NotificationManager* notification_manager_ = nullptr;
      FrameProvider* frame_ = nullptr;

      std::vector<Entity> updatable_entities_;
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

    public:
      void prepare() override;
      void update() override;
      void cleanup() override;
    };

    class CameraSystem final : public SceneSystem, public NonCopyable<CameraSystem> {
      Entity active_camera_{3};  // entity id of the active camera
      float32 aspect_ratio_{1.f};

    public:
      void set_active_camera(const Entity camera) { active_camera_ = camera; }
      Entity get_active_camera() const { return active_camera_; }

      void prepare() override;
      void update_cameras(float delta_time, const UserInput& movement, float aspect);
      void update() override;
      void cleanup() override;
    };

    class TransformSystem final : public SceneSystem, public NonCopyable<TransformSystem> {
      void mark_children_bounds_dirty(Entity entity);
      void mark_bounds_as_dirty(Entity entity);
      void update_aabb(Entity entity, const TransformComponent& parent_transform);
      void mark_parent_bounds_dirty(Entity entity);

    public:
      static glm::mat4 get_model_matrix(const TransformComponent& transform);

      void prepare() override;
      void update() override;
      void cleanup() override;
    };

    class AnimationSystem final : public SceneSystem, public NonCopyable<AnimationSystem> {
      float delta_time_ = 0.0f;

    public:
      void set_delta_time(float32 delta_time) { delta_time_ = delta_time; }

      void prepare() override;
      void update_translation(const Entity& entity,
                              AnimationChannel<glm::vec3>& translation_channel, bool loop) const;
      void update_rotation(const Entity& entity, AnimationChannel<glm::quat>& rotation_channel,
                           bool loop) const;
      void update() override;
      void cleanup() override;
    };

    class MeshSystem final : public SceneSystem, public NonCopyable<MeshSystem> {
      size_t meshes_ = 0;
      void traverse_scene(Entity entity, const TransformComponent& parent_transform);
      void upload_mesh();

      void create_buffers();
      void fill_descriptors();

    public:
      void prepare() override;
      void update() override;
      void cleanup() override;
    };

    class PhysicSystem final : public SceneSystem, public NonCopyable<PhysicSystem> {
      std::unique_ptr<PhysicEngine> physic_engine_;
      Entity player_ = invalid_entity;
    public:
      void prepare() override;
      void move_player(float delta_time, const UserInput& movement) const;
      void update(float delta_time, const UserInput& movement) const;
      void update() override;
      void cleanup() override;
    };

}  // namespace gestalt