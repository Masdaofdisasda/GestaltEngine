#pragma once
#include "Component.hpp"
#include "common.hpp"

namespace gestalt::foundation {

  struct TransformComponent : Component {
  private:
    glm::vec3 pos;
    glm::quat rot;
    float32 s;  // uniform scale for now

  public:
    TransformComponent()
        : pos(0),
          rot(1, 0, 0, 0),
          s(1),
          parent_position(0),
          parent_rotation(1, 0, 0, 0),
          parent_scale(1) {}
    TransformComponent(const glm::vec3& position, const glm::quat& rotation, const float32 scale)
        : pos(position),
          rot(rotation),
          s(scale),
          parent_position(0),
          parent_rotation(1, 0, 0, 0),
          parent_scale(1) {}
    TransformComponent(const glm::vec3& position, const glm::quat& rotation, const float32 scale,
                       const glm::vec3& parent_position, const glm::quat& parent_rotation,
                       const float32 parent_scale)
        : pos(position),
          rot(rotation),
          s(scale),
          parent_position(parent_position),
          parent_rotation(parent_rotation),
          parent_scale(parent_scale) {}

    glm::vec3 position() const { return pos; }
    glm::quat rotation() const { return rot; }
    glm::vec3 scale() const { return glm::vec3(s); }
    float32 scale_uniform() const { return s; }
    void set_position(const glm::vec3& new_position) { pos = new_position; }
    void set_rotation(const glm::quat& new_rotation) { rot = normalize(new_rotation); }
    void set_scale(const float32& new_scale) { s = new_scale; }
    void set_scale(const glm::vec3& new_scale) { s = new_scale.x; }
    // TODO evulate if we need to keep this
    glm::vec3 parent_position;
    glm::quat parent_rotation;
    float32 parent_scale;
    TransformComponent operator*(const TransformComponent& local_transform) const {
      TransformComponent result;

      // Compute the combined world position
      result.pos = pos + rot * (s * local_transform.pos);

      // Compute the combined world rotation
      result.rot = rot * local_transform.rot;

      // Compute the combined world scale
      result.s = s * local_transform.s;

      return result;
    }
  };

}  // namespace gestalt::foundation