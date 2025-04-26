#pragma once

#include "Component.hpp"
#include "UserInput.hpp"
#include "glm/vec3.hpp"
#include "glm/gtc/quaternion.hpp"

namespace gestalt::foundation {

  struct FirstPersonCameraComponent : Component {
  private:
      glm::vec3 position_;
      glm::quat orientation_ = glm::quat(glm::vec3(0.0f));
      glm::vec3 up_;

      float32 mouse_speed_ = 2.5f;  // Adjust based on your desired sensitivity

      // State Variables
      glm::vec2 mouse_pos_ = glm::vec2(0.0f);
  public:

      FirstPersonCameraComponent(const glm::vec3& start_position, const glm::vec3& start_up) {
      set_position(start_position);
      set_orientation(start_position + glm::vec3(0.0f, 0.0f, -1.0f), start_up);
      set_up_vector(start_up);
    }

      glm::vec3 position() const { return position_; }
      void set_position(const glm::vec3& pos) { position_ = pos; }

    glm::quat orientation() const { return orientation_; }
      void set_orientation(const glm::vec3& target, const glm::vec3& up) {
        orientation_ = quat_cast(lookAtRH(position_, target, up));
      }

    float32 mouse_speed() const { return mouse_speed_; }
      void set_mouse_speed(float32 speed) { mouse_speed_ = speed; }

      void set_up_vector(const glm::vec3& up) {
        this->up_ = up;
        const glm::mat4 view = view_matrix();
        const glm::vec3 dir = -glm::vec3(view[0][2], view[1][2], view[2][2]);
        orientation_ = lookAtRH(position_, position_ + dir, up);
      }

      [[nodiscard]] glm::mat4 view_matrix() const {
        const glm::mat4 t = translate(glm::mat4(1.0f), -position_);
        const glm::mat4 r = mat4_cast(orientation_);
        return r * t;
      }

      void update(const UserInput& movement) {
        const auto mouse_pos
            = glm::vec2(movement.mouse_position_x_rel, movement.mouse_position_y_rel);

        const glm::vec2 delta = mouse_pos - mouse_pos;
        glm::quat deltaQuat = glm::quat(
            glm::vec3(mouse_speed_ * delta.y, mouse_speed_ * delta.x, 0.0f));
        glm::quat unclamped_rotation = deltaQuat * orientation_;
        const float32 pitch = glm::pitch(unclamped_rotation);
        const float32 yaw = glm::yaw(unclamped_rotation);

        if ((std::abs(yaw) >= 0.01
             || (std::abs(pitch) <= glm::half_pi<float>())))  // clamp y-rotation
          orientation_ = unclamped_rotation;

        orientation_ = normalize(orientation_);
        set_up_vector(up_);
      }

    };

}  // namespace gestalt