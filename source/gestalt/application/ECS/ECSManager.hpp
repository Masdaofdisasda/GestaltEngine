#pragma once

#include <filesystem>

#include "BaseSystem.hpp"
#include "AnimationSystem.hpp"
#include "CameraSystem.hpp"
#include "ComponentFactory.hpp"
#include "MaterialSystem.hpp"
#include "PhysicSystem.hpp"
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
    class ECSManager {
      IGpu* gpu_ = nullptr;
      IResourceAllocator* resource_allocator_ = nullptr;
      Repository* repository_ = nullptr;

      std::unique_ptr<AssetLoader> asset_loader_;
      std::unique_ptr<ComponentFactory> component_factory_;

      std::unique_ptr<MaterialSystem> material_system_;
      std::unique_ptr<BaseSystem> light_system_;
      std::unique_ptr<CameraSystem> camera_system_;
      std::unique_ptr<BaseSystem> transform_system_;
      std::unique_ptr<AnimationSystem> animation_system_;
      std::unique_ptr<BaseSystem> mesh_system_;
      std::unique_ptr<PhysicSystem> physics_system_;
      std::unique_ptr<BaseSystem> audio_system_;
      std::unique_ptr<BaseSystem> raytracing_system_;

      Entity root_entity_ = 0;
      std::filesystem::path scene_path_;

    public:
      ECSManager() = default;
      ~ECSManager() = default;

      ECSManager(const ECSManager&) = delete;
      ECSManager& operator=(const ECSManager&) = delete;

      ECSManager(ECSManager&&) = delete;
      ECSManager& operator=(ECSManager&&) = delete;

      void init(IGpu* gpu,
                IResourceAllocator* resource_allocator,
                Repository* repository, FrameProvider* frame);
      void cleanup() const;

      void update_scene(float delta_time, const UserInput& movement, float aspect);

      void request_scene(const std::filesystem::path& file_path);
      [[nodiscard]] ComponentFactory& get_component_factory() const { return *component_factory_; }
      NodeComponent& get_root_node();
      [[nodiscard]] uint32 get_root_entity() const { return root_entity_; }
      void add_to_root(Entity entity, NodeComponent& node);
      void set_active_camera(Entity camera) const;
      [[nodiscard]] Entity get_active_camera() const;
    };

}  // namespace gestalt::application