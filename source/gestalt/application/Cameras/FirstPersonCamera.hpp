#pragma once

#include "common.hpp"


namespace gestalt::foundation {
  struct FirstPersonCameraData;
  struct UserInput;
};  // namespace gestalt::foundation

namespace gestalt::application {

  class FirstPersonCamera final {
  public:
    static void update(float32 delta_seconds, const UserInput& movement, FirstPersonCameraData& data);
  };


}  // namespace gestalt::application