#pragma once

#include "Component.hpp"
#include "glm/vec3.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/transform.hpp"

namespace gestalt::foundation {

  struct AnimationCameraComponent : Component {
  private:
      glm::vec3 position_ = glm::vec3(0.0f);
      glm::quat orientation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
  public:
      AnimationCameraComponent(const glm::vec3& starting_pos,
                             const glm::quat& starting_orientation)
        : position_(starting_pos),
          orientation_(starting_orientation) {}


      [[nodiscard]] glm::mat4 view_matrix() const {
        glm::mat4 world_transform
            = glm::translate(glm::mat4(1.0f), position_) * glm::mat4_cast(orientation_);
        return glm::inverse(world_transform);
      }

    glm::vec3 position() const { return position_; }
      void set_position(const glm::vec3& new_position) { position_ = new_position; }

    glm::quat orientation() const { return orientation_; }
      void set_orientation(const glm::quat& new_orientation) { orientation_ = new_orientation; }

      void set_euler_angles(float pitch, float yaw, float roll) {
        orientation_ = glm::quat(glm::vec3(yaw, pitch, roll));
      }

      void update() {
        orientation_ = normalize(orientation_);
      }

    };

}  // namespace gestalt