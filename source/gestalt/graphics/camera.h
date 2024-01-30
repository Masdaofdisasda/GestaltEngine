#pragma once

#include "input_system.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/transform.hpp>

class camera_positioner_interface {
public:
  virtual ~camera_positioner_interface() = default;

  virtual void init(const glm::vec3& pos, const glm::vec3& target, const glm::vec3& up) = 0;

  [[nodiscard]] virtual glm::mat4 get_view_matrix() const = 0;

  [[nodiscard]] virtual glm::vec3 get_position() const = 0;

  [[nodiscard]] virtual glm::quat get_orientation() const = 0;

  virtual void update(double delta_seconds, const movement& movement) = 0;
};


class camera final {
public:
  camera() : positioner_(nullptr) {}

  void init(camera_positioner_interface& positioner) { positioner_ = &positioner; }

  camera(const camera&) = default;
  camera& operator=(const camera&) = default;

  glm::mat4 get_view_matrix() const { return positioner_->get_view_matrix(); }
  glm::vec3 get_position() const { return positioner_->get_position(); }
  glm::quat get_orientation() const { return positioner_->get_orientation(); }
  void set_positioner(camera_positioner_interface* new_positioner) { positioner_ = new_positioner; }
  void update(double delta_seconds, const movement& movement) const {
    positioner_->update(delta_seconds, movement);
  }

private:
  camera_positioner_interface* positioner_;
};

class free_fly_camera final : public camera_positioner_interface {

public:

  void init(const glm::vec3& pos, const glm::vec3& target, const glm::vec3& up) override {
    camera_position_ = pos;
    camera_orientation_ = quatLookAt(normalize(target - pos), up);
    up_ = up;
  }

  void update(double delta_seconds, const movement& movement) override;

  glm::mat4 get_view_matrix() const override {
    const glm::mat4 t = translate(glm::mat4(1.0f), -camera_position_);
    const glm::mat4 r = mat4_cast(camera_orientation_);
    return r * t;
  }

  glm::vec3 get_position() const override { return camera_position_; }

  void set_position(const glm::vec3& pos);

  void reset_mouse_position(const glm::vec2& p) { mouse_pos_ = p; }

  void set_up_vector(const glm::vec3& up);

  glm::quat get_orientation() const override { return camera_orientation_; }

private:
  float mouse_speed = 4.5f;
  float acceleration = .01f;
  float damping = 15.f;    // changes deceleration speed
  float max_speed = .05f;  // clamps movement
  float fast_coef = 5.f;  // l-ctrl mode uses this
  float slow_coef = .001f;  // l-alt mode uses this

  glm::vec2 mouse_pos_ = glm::vec2(0);
  glm::vec3 camera_position_ = glm::vec3(0.0f, 0.0f, 5.0f);
  glm::vec3 up_ = glm::vec3(0.0f, 1.0f, 1.0f);
  glm::quat camera_orientation_ = glm::quat(glm::vec3(0.0f, 0.0f, 0.0f));
  glm::vec3 move_speed_ = glm::vec3(0.0f);
};
