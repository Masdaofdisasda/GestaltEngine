#pragma once
#include "common.hpp"

namespace gestalt::foundation {

  struct SpotLightData {
    float32 range;
    uint32 light_view_projection;  // index into the light view projection buffer
    float32 inner_cone_cos;
    float32 outer_cone_cos;
  };

}  // namespace gestalt::foundation