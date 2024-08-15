#pragma once

#include "common.hpp"
#include "Components.hpp"


namespace gestalt::foundation {
  struct Movement;

  class FreeFlyCamera final {
  public:
    static void update(float64 delta_seconds, const Movement& movement, FreeFlyCameraData& data);
  };

  class OrbitCamera final {
  public:
    static void update(float64 delta_seconds, const Movement& movement, OrbitCameraData& data);
  };

  class FirstPersonCamera final {
  public:
    static void update(float64 delta_seconds, const Movement& movement, FirstPersonCameraData& data);
  };

  class MoveToCamera final {
  public:
    static void update(float64 delta_seconds, const Movement& movement, AnimationCameraData& data);
  };

}  // namespace gestalt::foundation