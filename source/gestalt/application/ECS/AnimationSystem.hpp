#pragma once

#include "Repository.hpp"
#include "BaseSystem.hpp"
#include "Animation/AnimationChannel.hpp"

namespace gestalt::application {

    class AnimationSystem final : public BaseSystem {
      float delta_time_ = 0.0f;
      void update_translation(const Entity& entity,
                              AnimationChannel<glm::vec3>& translation_channel, bool loop) const;
      void update_rotation(const Entity& entity, AnimationChannel<glm::quat>& rotation_channel,
                           bool loop) const;

    public:
      AnimationSystem() = default;
      ~AnimationSystem() override = default;

      AnimationSystem(const AnimationSystem&) = delete;
      AnimationSystem& operator=(const AnimationSystem&) = delete;

      AnimationSystem(AnimationSystem&&) = delete;
      AnimationSystem& operator=(AnimationSystem&&) = delete;

      void update(float delta_time, const UserInput& movement, float aspect) override;
    };

}  // namespace gestalt