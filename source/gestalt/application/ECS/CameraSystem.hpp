#pragma once
#include "Components/Entity.hpp"

namespace gestalt::application {
  class EventBus;
}

namespace gestalt::foundation {
  class Repository;
  class IGpu;
  struct FrameProvider;
  class IResourceAllocator;
  struct UserInput;
}

namespace gestalt::application {

  class CameraSystem final {
    IGpu& gpu_;
    IResourceAllocator& resource_allocator_;
    Repository& repository_;
    FrameProvider& frame_;
    EventBus& event_bus_;

    Entity active_camera_{0};  // entity id of the active camera
    float32 aspect_ratio_{1.f};

  public:
    CameraSystem(IGpu& gpu, IResourceAllocator& resource_allocator, Repository& repository,
                 FrameProvider& frame, EventBus& event_bus);
    ~CameraSystem();

    CameraSystem(const CameraSystem&) = delete;
    CameraSystem& operator=(const CameraSystem&) = delete;

    CameraSystem(CameraSystem&&) = delete;
    CameraSystem& operator=(CameraSystem&&) = delete;

    void set_active_camera(const Entity camera) { active_camera_ = camera; }
    [[nodiscard]] Entity get_active_camera() const { return active_camera_; }

    void update(float delta_time, const UserInput& movement, float aspect);
  };

}  // namespace gestalt::application