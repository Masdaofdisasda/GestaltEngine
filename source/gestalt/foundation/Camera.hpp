#pragma once

#include "common.hpp"

#include <glm/gtc/quaternion.hpp>


namespace gestalt::foundation {
    struct Movement;

    class CameraPositioner {
    public:
      virtual ~CameraPositioner() = default;

      [[nodiscard]] virtual glm::mat4 get_view_matrix() const = 0;

      [[nodiscard]] virtual glm::vec3 get_position() const = 0;

      [[nodiscard]] virtual glm::quat get_orientation() const = 0;

      virtual void update(float64 delta_seconds, const Movement& movement) = 0;
    };

    class Camera final {
    public:
      Camera(CameraPositioner& positioner) : positioner_(&positioner) {}

      Camera(const Camera&) = default;
      Camera& operator=(const Camera&) = default;

      [[nodiscard]] glm::mat4 get_view_matrix() const { return positioner_->get_view_matrix(); }
      [[nodiscard]] glm::vec3 get_position() const { return positioner_->get_position(); }
      [[nodiscard]] glm::quat get_orientation() const { return positioner_->get_orientation(); }
      void set_positioner(CameraPositioner* new_positioner) {
        positioner_ = new_positioner;
      }
      void update(float64 delta_seconds, const Movement& movement) const {
        positioner_->update(delta_seconds, movement);
      }

    private:
      CameraPositioner* positioner_;
    };

    class FreeFlyCamera final : public CameraPositioner {
    public:
      FreeFlyCamera(const glm::vec3& pos, const glm::vec3& target, const glm::vec3& up) {
        set_position(pos);
        set_orientation(target, up);
        set_up_vector(up);
      }

      void update(float64 delta_seconds, const Movement& movement) override;

      [[nodiscard]] glm::mat4 get_view_matrix() const override;

      [[nodiscard]] glm::vec3 get_position() const override;

      void set_position(const glm::vec3& pos);
      void set_orientation(const glm::vec3& target, const glm::vec3& up);

      void reset_mouse_position(const glm::vec2& p) { mouse_pos_ = p; }

      void set_up_vector(const glm::vec3& up);

      [[nodiscard]] glm::quat get_orientation() const override { return camera_orientation_; }

    private:
      float32 mouse_speed = 4.5f;
      float32 acceleration = .01f;
      float32 damping = 15.f;   // changes deceleration speed
      float32 max_speed = .05f;  // clamps movement
      float32 fast_coef = 5.f;   // l-ctrl mode uses this
      float32 slow_coef = .001f;  // l-alt mode uses this

      glm::vec2 mouse_pos_ = glm::vec2(0);
      glm::vec3 camera_position_ = glm::vec3(0.0f, 0.0f, 5.0f);
      glm::vec3 up_ = glm::vec3(0.0f, 1.0f, 1.0f);
      glm::quat camera_orientation_ = glm::quat(glm::vec3(0.0f, 0.0f, 0.0f));
      glm::vec3 move_speed_ = glm::vec3(0.0f);
    };

  class OrbitCamera final : public CameraPositioner {
    public:
      OrbitCamera(const glm::vec3& target, float distance, float minDistance, float maxDistance)
          : target_(target),
            distance_(distance),
            minDistance_(minDistance),
            maxDistance_(maxDistance) {
        update_orientation();
      }

      void update(float64 delta_seconds, const Movement& movement) override;

      [[nodiscard]] glm::mat4 get_view_matrix() const override;

      [[nodiscard]] glm::vec3 get_position() const override;

      [[nodiscard]] glm::quat get_orientation() const override;

      void set_target(const glm::vec3& target);

      void set_distance(float distance);

      void set_up_vector(const glm::vec3& up);

    private:
      void update_orientation();

      glm::vec3 target_;
      float distance_;
      float minDistance_;
      float maxDistance_;

      float yaw_ = 45.0f;
      float pitch_ = .3f;
      glm::vec3 position_ = glm::vec3(0.0f);
      glm::quat orientation_ = glm::quat(glm::vec3(0.0f));
      glm::vec3 up_ = glm::vec3(0.0f, 1.0f, 0.0f);

      glm::mat4 view_matrix_ = glm::mat4(1.0f);
      glm::vec2 mouse_pos_ = glm::vec2(0);

      // Configuration parameters
      float orbit_speed_ = 2.f;
      float zoom_speed_ = 0.1f;
      float pan_speed_ = 10.0f;
    };



}  // namespace gestalt::application