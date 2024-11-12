#pragma once

#include "common.hpp"


namespace gestalt::foundation {
  struct OrbitCameraData;
  struct UserInput;
};  // namespace gestalt::foundation

namespace gestalt::application {


  class OrbitCamera final {
  public:
    static void update(float32 delta_seconds, const UserInput& movement, OrbitCameraData& data);
  };


}  // namespace gestalt::application