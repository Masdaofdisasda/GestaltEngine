#pragma once

namespace gestalt::foundation {
  
    struct AABB {
      glm::vec3 min{glm::vec3(std::numeric_limits<float32>::max())};
      glm::vec3 max{glm::vec3(std::numeric_limits<float32>::lowest())};

      mutable bool is_dirty = true;
    };
}  // namespace gestalt