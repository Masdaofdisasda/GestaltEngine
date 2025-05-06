#pragma once

#include "Animation/AnimationChannel.hpp"
#include "Components/Entity.hpp"
#include <glm/fwd.hpp>

namespace gestalt::application {
  class EventBus;
}

namespace gestalt::foundation {
  class Repository;
}

namespace gestalt::application {

  class AnimationSystem final {
    Repository& repository_;
    EventBus& event_bus_;
    float delta_time_ = 0.0f;
    glm::vec3 update_translation(AnimationChannel<glm::vec3>& translation_channel,
                            bool loop) const;
    glm::quat update_rotation(AnimationChannel<glm::quat>& rotation_channel,
                         bool loop) const;

  public:
    explicit AnimationSystem(Repository& repository, EventBus& event_bus);
    ~AnimationSystem() = default;

    AnimationSystem(const AnimationSystem&) = delete;
    AnimationSystem& operator=(const AnimationSystem&) = delete;

    AnimationSystem(AnimationSystem&&) = delete;
    AnimationSystem& operator=(AnimationSystem&&) = delete;

    void update(float delta_time);
  };

}  // namespace gestalt::application