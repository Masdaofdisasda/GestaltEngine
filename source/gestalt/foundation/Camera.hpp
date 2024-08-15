#pragma once

#include "common.hpp"
#include "Components.hpp"

#include <glm/gtc/quaternion.hpp>


namespace gestalt::foundation {
  struct Movement;

  class CameraPositioner {
  public:
    virtual ~CameraPositioner() = default;

    [[nodiscard]] virtual glm::mat4 get_view_matrix(CameraData& data) const = 0;

    virtual void update(float64 delta_seconds, const Movement& movement, CameraData& data) = 0;
  };

  class Camera final {
  public:
    explicit Camera(CameraPositioner& positioner) : positioner_(&positioner) {}

    Camera(const Camera&) = default;
    Camera& operator=(const Camera&) = default;

    [[nodiscard]] glm::mat4 get_view_matrix(CameraData& data) const {
      return positioner_->get_view_matrix(data);
    }
    void set_positioner(CameraPositioner* new_positioner) { positioner_ = new_positioner; }
    void update(float64 delta_seconds, const Movement& movement, CameraData& data) const {
      positioner_->update(delta_seconds, movement, data);
    }

  private:
    CameraPositioner* positioner_;
  };

  class FreeFlyCamera final : public CameraPositioner {
  public:
    void update(float64 delta_seconds, const Movement& movement, CameraData& data) override;

    [[nodiscard]] glm::mat4 get_view_matrix(CameraData& data) const override;
  };

  class OrbitCamera final : public CameraPositioner {
  public:
    void update(float64 delta_seconds, const Movement& movement, CameraData& data) override;

    [[nodiscard]] glm::mat4 get_view_matrix(CameraData& data) const override;
  };

  class FirstPersonCamera final : public CameraPositioner {
  public:
    void update(float64 delta_seconds, const Movement& movement, CameraData& data) override;

    [[nodiscard]] glm::mat4 get_view_matrix(CameraData& data) const override;
  };

  class MoveToCamera final : public CameraPositioner {
  public:
    void update(float64 delta_seconds, const Movement& movement, CameraData& data) override;

    glm::mat4 get_view_matrix(CameraData& data) const override;
  };

}  // namespace gestalt::foundation