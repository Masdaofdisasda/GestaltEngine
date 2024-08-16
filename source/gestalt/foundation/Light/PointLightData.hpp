#pragma once
#include "common.hpp"

namespace gestalt::foundation {

      struct PointLightData {
        float32 range;
        uint32 first_light_view_projection; // index into the light view projection buffer the first out of 6 faces of the cube map
      };

}  // namespace gestalt