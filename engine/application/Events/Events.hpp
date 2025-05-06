#pragma once
#include "Components/Entity.hpp"
#include "glm/fwd.hpp"

namespace gestalt::application {

  struct MoveEntityEvent {
    MoveEntityEvent(Entity entity, glm::vec3 new_position, glm::quat new_rotation,
                    float32 new_scale)
        : entity_(entity),
          new_position_(new_position),
          new_rotation_(new_rotation),
          new_scale_(new_scale) {}
    [[nodiscard]] Entity entity() const { return entity_; }
    [[nodiscard]] glm::vec3 new_position() const { return new_position_; }
    [[nodiscard]] glm::quat new_rotation() const { return new_rotation_; }
    [[nodiscard]] glm::vec3 new_scale() const { return new_scale_; }

  private:
    Entity entity_;
    glm::vec3 new_position_;
    glm::quat new_rotation_;
    glm::vec3 new_scale_;
  };

  struct TranslateEntityEvent {
    TranslateEntityEvent(Entity entity, glm::vec3 new_position)
        : entity_(entity), new_position_(new_position) {}

    [[nodiscard]] Entity entity() const { return entity_; }
    [[nodiscard]] glm::vec3 new_position() const { return new_position_; }

  private:
    Entity entity_;
    glm::vec3 new_position_;
  };

  struct RotateEntityEvent {
    RotateEntityEvent(Entity entity, glm::quat new_rotation)
        : entity_(entity), new_rotation_(new_rotation) {}
    [[nodiscard]] Entity entity() const { return entity_; }
    [[nodiscard]] glm::quat new_rotation() const { return new_rotation_; }

  private:
    Entity entity_;
    glm::quat new_rotation_;
  };

  struct ScaleEntityEvent {
    ScaleEntityEvent(Entity entity, float32 new_scale) : entity_(entity), new_scale_(new_scale) {}

    [[nodiscard]] Entity entity() const { return entity_; }
    [[nodiscard]] float32 new_scale() const { return new_scale_; }

  private:
    Entity entity_;
    float32 new_scale_;
  };

  struct UpdateLightEvent {
    UpdateLightEvent(Entity entity, glm::vec3 color, float32 intensity)
        : entity_(entity), color_(color), intensity_(intensity) {}

    [[nodiscard]] Entity entity() const { return entity_; }
    [[nodiscard]] glm::vec3 color() const { return color_; }
    [[nodiscard]] float32 intensity() const { return intensity_; }

  private:
    Entity entity_;
    glm::vec3 color_;
    float32 intensity_;
  };

  struct UpdatePointLightEvent {
    UpdatePointLightEvent(Entity entity, float32 range) : entity_(entity), range_(range) {}
    [[nodiscard]] Entity entity() const { return entity_; }
    [[nodiscard]] float32 range() const { return range_; }

  private:
    Entity entity_;
    float32 range_;
  };

  struct UpdateSpotLightEvent {
    UpdateSpotLightEvent(Entity entity, float32 range, float32 inner_cos, float32 outer_cos)
        : entity_(entity), range_(range), inner_cos_(inner_cos), outer_cos_(outer_cos) {}

    [[nodiscard]] Entity entity() const { return entity_; }
    [[nodiscard]] float32 range() const { return range_; }
    [[nodiscard]] float32 inner_cos() const { return inner_cos_; }
    [[nodiscard]] float32 outer_cos() const { return outer_cos_; }

  private:
    Entity entity_;
    float32 range_;
    float32 inner_cos_;
    float32 outer_cos_;
  };

  struct UpdatePerspectiveProjectionEvent {
    UpdatePerspectiveProjectionEvent(Entity entity, float32 fov, float32 near, float32 far)
        : entity_(entity), fov_(fov), near_(near), far_(far) {}

    [[nodiscard]] Entity entity() const { return entity_; }
    [[nodiscard]] float32 fov() const { return fov_; }
    [[nodiscard]] float32 near() const { return near_; }
    [[nodiscard]] float32 far() const { return far_; }

  private:
    Entity entity_;
    float32 fov_, near_, far_;
  };

  struct UpdateOrthographicProjectionEvent {
    UpdateOrthographicProjectionEvent(Entity entity, float32 left, float32 right, float32 bottom,
                                      float32 top, float32 near, float32 far)
        : entity_(entity),
          left_(left),
          right_(right),
          bottom_(bottom),
          top_(top),
          near_(near),
          far_(far) {}

    [[nodiscard]] Entity entity() const { return entity_; }
    [[nodiscard]] float32 left() const { return left_; }
    [[nodiscard]] float32 right() const { return right_; }
    [[nodiscard]] float32 bottom() const { return bottom_; }
    [[nodiscard]] float32 top() const { return top_; }
    [[nodiscard]] float32 near() const { return near_; }
    [[nodiscard]] float32 far() const { return far_; }

  private:
    Entity entity_;
    float32 left_, right_, bottom_, top_, near_, far_;
  };

  struct UpdateFirstPersonCameraEvent {
    UpdateFirstPersonCameraEvent(Entity entity, float32 mouse_speed)
        : entity_(entity), mouse_speed_(mouse_speed) {}

    [[nodiscard]] Entity entity() const { return entity_; }
    [[nodiscard]] float32 mouse_speed() const { return mouse_speed_; }

  private:
    Entity entity_;
    float32 mouse_speed_;
  };

  struct UpdateFreeFlyCameraEvent {
    UpdateFreeFlyCameraEvent(Entity entity, float32 mouse_speed, float32 acceleration,
                             float32 damping, float32 max_speed, float32 fast_coef,
                             float32 slow_coef)
        : entity_(entity),
          mouse_speed_(mouse_speed),
          acceleration_(acceleration),
          damping_(damping),
          max_speed_(max_speed),
          fast_coef_(fast_coef),
          slow_coef_(slow_coef) {}

    [[nodiscard]] Entity entity() const { return entity_; }
    [[nodiscard]] float32 mouse_speed() const { return mouse_speed_; }
    [[nodiscard]] float32 acceleration() const { return acceleration_; }
    [[nodiscard]] float32 damping() const { return damping_; }
    [[nodiscard]] float32 max_speed() const { return max_speed_; }
    [[nodiscard]] float32 fast_coef() const { return fast_coef_; }
    [[nodiscard]] float32 slow_coef() const { return slow_coef_; }

  private:
    Entity entity_;

    float32 mouse_speed_, acceleration_, damping_, max_speed_, fast_coef_, slow_coef_;
  };

  struct UpdateOrbitCameraEvent {
    UpdateOrbitCameraEvent(Entity entity, float32 distance, float32 yaw, float32 pitch,
                           float32 orbit_speed, float32 zoom_speed, float32 pan_speed)
        : entity_(entity),
          distance_(distance),
          yaw_(yaw),
          pitch_(pitch),
          orbit_speed_(orbit_speed),
          zoom_speed_(zoom_speed),
          pan_speed_(pan_speed) {}

    [[nodiscard]] Entity entity() const { return entity_; }
    [[nodiscard]] float32 distance() const { return distance_; }
    [[nodiscard]] float32 yaw() const { return yaw_; }
    [[nodiscard]] float32 pitch() const { return pitch_; }
    [[nodiscard]] float32 orbit_speed() const { return orbit_speed_; }
    [[nodiscard]] float32 zoom_speed() const { return zoom_speed_; }
    [[nodiscard]] float32 pan_speed() const { return pan_speed_; }

  private:
    Entity entity_;
    float32 distance_, yaw_, pitch_, orbit_speed_, zoom_speed_, pan_speed_;
  };
}  // namespace gestalt::application