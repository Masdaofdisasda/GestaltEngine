#pragma once

#include "common.hpp"

namespace gestalt::foundation {

  struct PerspectiveProjectionData {
    PerspectiveProjectionData(float32 fov, float32 aspect_ratio, float32 near, float32 far)
        : fov(fov), aspect_ratio(aspect_ratio), near(near), far(far) {}

    float32 fov;
    float32 aspect_ratio;
    float32 near;
    float32 far;
  };
}  // namespace gestalt