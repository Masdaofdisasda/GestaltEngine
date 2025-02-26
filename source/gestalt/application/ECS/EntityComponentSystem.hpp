#pragma once

#include <filesystem>

#include "BaseSystem.hpp"
#include "AnimationSystem.hpp"
#include "AudioSystem.hpp"
#include "CameraSystem.hpp"
#include "ComponentFactory.hpp"
#include "LightSystem.hpp"
#include "MaterialSystem.hpp"
#include "MeshSystem.hpp"
#include "PhysicSystem.hpp"
#include "RayTracingSystem.hpp"
#include "TransformSystem.hpp"
#include "common.hpp"
#include "Resource Loading/AssetLoader.hpp"


namespace gestalt::foundation {
  class IResourceAllocator;
  struct UserInput;
  class IDescriptorLayoutBuilder;
  class IGpu;
}

namespace gestalt::application {

    /**
     * @brief Class responsible for managing scenes, entities, and their components.
     */
    class EntityComponentSystem {
      IGpu& gpu_;
      Repository& repository_;

      ComponentFactory component_factory_;
      AssetLoader asset_loader_;

      MaterialSystem material_system_;
      CameraSystem camera_system_;
      LightSystem light_system_;
      TransformSystem transform_system_;
      AnimationSystem animation_system_;
      MeshSystem mesh_system_;
      AudioSystem audio_system_;
      PhysicSystem physics_system_;
      RayTracingSystem raytracing_system_;

      Entity root_entity_ = 0;
      std::filesystem::path scene_path_;

    public:
      EntityComponentSystem(IGpu& gpu, IResourceAllocator& resource_allocator, Repository& repository,
                 FrameProvider& frame);
      ~EntityComponentSystem();

      EntityComponentSystem(const EntityComponentSystem&) = delete;
      EntityComponentSystem& operator=(const EntityComponentSystem&) = delete;

      EntityComponentSystem(EntityComponentSystem&&) = delete;
      EntityComponentSystem& operator=(EntityComponentSystem&&) = delete;

      void update_scene(float delta_time, const UserInput& movement, float aspect);

      void request_scene(const std::filesystem::path& file_path);
      [[nodiscard]] ComponentFactory& get_component_factory() { return component_factory_; }
      NodeComponent& get_root_node();
      [[nodiscard]] uint32 get_root_entity() const { return root_entity_; }
      void add_to_root(Entity entity, NodeComponent& node);
      void set_active_camera(Entity camera);
      [[nodiscard]] Entity get_active_camera() const;
    };

}  // namespace gestalt::application