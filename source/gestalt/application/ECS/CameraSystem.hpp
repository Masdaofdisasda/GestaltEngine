#pragma once

#include "Repository.hpp"
#include "BaseSystem.hpp"

namespace gestalt::foundation {
  struct UserInput;
}

namespace gestalt::application {

    class CameraSystem final : public BaseSystem, public NonCopyable<CameraSystem> {
      Entity active_camera_{0};  // entity id of the active camera
      float32 aspect_ratio_{1.f};

    public:
      void set_active_camera(const Entity camera) { active_camera_ = camera; }
      Entity get_active_camera() const { return active_camera_; }

      void prepare() override;
      void update(float delta_time, const UserInput& movement, float aspect) override;
      void cleanup() override;
    };

}  // namespace gestalt