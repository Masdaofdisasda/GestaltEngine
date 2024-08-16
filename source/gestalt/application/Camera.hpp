#pragma once

#include "common.hpp"


namespace gestalt::foundation {
  struct AnimationCameraData;
  struct FirstPersonCameraData;
  struct OrbitCameraData;
  struct FreeFlyCameraData;
  struct UserInput;
};  // namespace gestalt::foundation

namespace gestalt::application {

  class FreeFlyCamera final {
  public:
    static void update(float64 delta_seconds, const UserInput& movement, FreeFlyCameraData& data);
  };

  class OrbitCamera final {
  public:
    static void update(float64 delta_seconds, const UserInput& movement, OrbitCameraData& data);
  };

  class FirstPersonCamera final {
  public:
    static void update(float64 delta_seconds, const UserInput& movement, FirstPersonCameraData& data);
  };

  class MoveToCamera final {
  public:
    static void update(float64 delta_seconds, const UserInput& movement, AnimationCameraData& data);
  };

}  // namespace gestalt::application