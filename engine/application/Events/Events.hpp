#pragma once
#include "Components/Entity.hpp"
#include "glm/fwd.hpp"

namespace gestalt::application {

  struct MoveEntityEvent {
    MoveEntityEvent(Entity entity, glm::vec3 new_position, glm::quat new_rotation, float32 new_scale)
        : entity(entity), new_position(new_position), new_rotation(new_rotation), new_scale(new_scale) {}
    Entity entity;
    glm::vec3 new_position;
    glm::quat new_rotation;
    glm::vec3 new_scale;
  };

  struct TranslateEntityEvent {
    TranslateEntityEvent(Entity entity, glm::vec3 new_position)
        : entity(entity), new_position(new_position) {}
    Entity entity;
    glm::vec3 new_position;
  };

  struct RotateEntityEvent {
    RotateEntityEvent(Entity entity, glm::quat new_rotation)
        : entity(entity), new_rotation(new_rotation) {}
    Entity entity;
    glm::quat new_rotation;
  };

  struct ScaleEntityEvent {
    ScaleEntityEvent(Entity entity, float32 new_scale) : entity(entity), new_scale(new_scale) {}
    Entity entity;
    float32 new_scale;
  };

  struct UpdateLightEvent {
    UpdateLightEvent(Entity entity, glm::vec3 color, float32 intensity)
        : entity(entity), color(color), intensity(intensity) {}
    Entity entity;
    glm::vec3 color;
    float32 intensity;
  };

  struct UpdatePointLightEvent {
    UpdatePointLightEvent(Entity entity, float32 range)
        : entity(entity), range(range) {}
    Entity entity;
    float32 range;
  };

  struct UpdateSpotLightEvent {
    UpdateSpotLightEvent(Entity entity, float32 range, float32 inner_cos, float32 outer_cos)
        : entity(entity),range(range), inner_cos(inner_cos), outer_cos(outer_cos) {}
    Entity entity;
    float32 range;
    float32 inner_cos;
    float32 outer_cos;
  };

  struct UpdatePerspectiveProjectionEvent {
    UpdatePerspectiveProjectionEvent(Entity entity, float32 fov, float32 near, float32 far)
        : entity(entity), fov(fov), near(near), far(far) {}
    Entity entity;
    float32 fov, near, far;
  };

  struct UpdateOrthographicProjectionEvent {
    UpdateOrthographicProjectionEvent(Entity entity, float32 left, float32 right, float32 bottom,
                                      float32 top, float32 near, float32 far)
        : entity(entity),
          left(left),
          right(right),
          bottom(bottom),
          top(top),
          near(near),
          far(far) {}
    Entity entity;
    float32 left, right, bottom, top, near, far;
  };

  struct UpdateFirstPersonCameraEvent {
    UpdateFirstPersonCameraEvent(Entity entity, float32 mouse_speed)
        : entity(entity), mouse_speed(mouse_speed) {}
    Entity entity;
    float32 mouse_speed;
  };

  struct UpdateFreeFlyCameraEvent {
    UpdateFreeFlyCameraEvent(Entity entity, float32 mouse_speed, float32 acceleration,
                             float32 damping, float32 max_speed, float32 fast_coef,
                             float32 slow_coef)
        : entity(entity),
          mouse_speed(mouse_speed),
          acceleration(acceleration),
          damping(damping),
          max_speed(max_speed),
          fast_coef(fast_coef),
          slow_coef(slow_coef) {}
    Entity entity;

    float32 mouse_speed, acceleration, damping, max_speed, fast_coef, slow_coef;
  };

  struct UpdateOrbitCameraEvent {
    UpdateOrbitCameraEvent(Entity entity, float32 distance, float32 yaw, float32 pitch,
                           float32 orbit_speed, float32 zoom_speed, float32 pan_speed)
        : entity(entity),
          distance(distance),
          yaw(yaw),
          pitch(pitch),
          orbit_speed(orbit_speed),
          zoom_speed(zoom_speed),
          pan_speed(pan_speed) {}
    Entity entity;
    float32 distance, yaw, pitch, orbit_speed, zoom_speed, pan_speed;
  };
}  // namespace gestalt::application