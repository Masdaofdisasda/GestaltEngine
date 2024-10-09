#pragma once
#include <glm/gtc/quaternion.hpp>

#include "AnimationChannel.hpp"
#include "common.hpp"

namespace gestalt::foundation {

  struct AnimationComponent {
    AnimationChannel<glm::vec3> translation_channel;
    AnimationChannel<glm::quat> rotation_channel;
    AnimationChannel<glm::vec3> scale_channel;
    bool loop = true;
    float32 current_time = 0.0f; // Current time in the animation

    explicit AnimationComponent(std::vector<Keyframe<glm::vec3>>& translation_keyframes,
                                std::vector<Keyframe<glm::quat>>& rotation_keyframes,
                                std::vector<Keyframe<glm::vec3>>& scale_keyframes)
        : translation_channel(AnimationChannel(translation_keyframes)),
          rotation_channel(AnimationChannel(rotation_keyframes)),
          scale_channel(AnimationChannel(scale_keyframes)) {}
  };

}  // namespace gestalt