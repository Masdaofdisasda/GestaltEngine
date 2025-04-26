#pragma once
#include "PbrMaterial.hpp"

namespace gestalt::foundation {

    struct Material {
      std::string name;
      PbrMaterial config;
      bool is_dirty = true;
    };
}  // namespace gestalt