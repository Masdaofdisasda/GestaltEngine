#pragma once

#include "common.hpp"


namespace gestalt::foundation {
  struct AnimationCameraData;
  struct UserInput;
};  // namespace gestalt::foundation

namespace gestalt::application {

  class AnimationCamera final {
  public:
    static void update(float32 delta_seconds, const UserInput& movement, AnimationCameraData& data);
  };

}  // namespace gestalt::application