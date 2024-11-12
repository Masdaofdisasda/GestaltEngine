#pragma once

#include "common.hpp"


namespace gestalt::foundation {
  struct FreeFlyCameraData;
  struct UserInput;
};  // namespace gestalt::foundation

namespace gestalt::application {

  class FreeFlyCamera final {
  public:
    static void update(float32 delta_seconds, const UserInput& movement, FreeFlyCameraData& data);
  };


}  // namespace gestalt::application