#pragma once

namespace gestalt::foundation {
  
    struct BoundingSphere {
      glm::vec3 center;
      float radius;
      mutable bool is_dirty = true;
    };
}  // namespace gestalt