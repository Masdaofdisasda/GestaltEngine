#pragma once

#include "Component.hpp"
#include "common.hpp"
#include "glm/mat4x4.hpp"
#include "glm/ext/matrix_clip_space.hpp"

namespace gestalt::foundation {

  struct PerspectiveProjectionComponent : Component {
  private:
    float32 fov_;  // in radians
    float32 near_;
    float32 far_;
    float32 aspect_ratio_;

  public:
    PerspectiveProjectionComponent(float32 fov, float32 near, float32 far, float32 aspect_ratio = 1.f)
        : fov_(fov), near_(near), far_(far), aspect_ratio_(aspect_ratio) {}

    glm::mat4 projection_matrix() const {
      return glm::perspective(fov_, aspect_ratio_, near_, far_);
    }

    float32 fov() const { return fov_; }
    void set_fov(float32 fov) { fov_ = fov; }

    float32 near() const { return near_; }
    void set_near(float32 near) { near_ = near; }

    float32 far() const { return far_; }
    void set_far(float32 far) { far_ = far; }

    void set_aspect_ratio(const float32 aspect_ratio) { aspect_ratio_ = aspect_ratio; }
  };
}  // namespace gestalt