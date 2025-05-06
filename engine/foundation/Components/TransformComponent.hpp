#pragma once
#include "Component.hpp"
#include "common.hpp"

namespace gestalt::foundation {

  struct TransformComponent : Component {
  private:
    glm::vec3 pos_;
    glm::quat rot_;
    float32 s_;  // uniform scale for now

  public:
    TransformComponent()
        : pos_(0),
          rot_(1, 0, 0, 0),
          s_(1),
          parent_position(0),
          parent_rotation(1, 0, 0, 0),
          parent_scale(1) {}
    TransformComponent(const glm::vec3& position, const glm::quat& rotation, const float32 scale)
        : pos_(position),
          rot_(rotation),
          s_(scale),
          parent_position(0),
          parent_rotation(1, 0, 0, 0),
          parent_scale(1) {}
    TransformComponent(const glm::vec3& position, const glm::quat& rotation, const float32 scale,
                       const glm::vec3& parent_position, const glm::quat& parent_rotation,
                       const float32 parent_scale)
        : pos_(position),
          rot_(rotation),
          s_(scale),
          parent_position(parent_position),
          parent_rotation(parent_rotation),
          parent_scale(parent_scale) {}

    glm::vec3 position() const { return pos_; }
    glm::quat rotation() const { return rot_; }
    glm::vec3 scale() const { return glm::vec3(s_); }
    float32 scale_uniform() const { return s_; }
    void set_position(const glm::vec3& new_position) { pos_ = new_position; }
    void set_rotation(const glm::quat& new_rotation) { rot_ = normalize(new_rotation); }
    void set_scale(const float32& new_scale) { s_ = new_scale; }
    void set_scale(const glm::vec3& new_scale) { s_ = new_scale.x; }
    // TODO evulate if we need to keep this
    glm::vec3 parent_position;
    glm::quat parent_rotation;
    float32 parent_scale;
    TransformComponent operator*(const TransformComponent& local_transform) const {
      TransformComponent result;

      // Compute the combined world position
      result.pos_ = pos_ + rot_ * (s_ * local_transform.pos_);

      // Compute the combined world rotation
      result.rot_ = rot_ * local_transform.rot_;

      // Compute the combined world scale
      result.s_ = s_ * local_transform.s_;

      return result;
    }
  };

}  // namespace gestalt::foundation