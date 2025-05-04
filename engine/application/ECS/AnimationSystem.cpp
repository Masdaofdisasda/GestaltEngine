#include "AnimationSystem.hpp"

#include "Repository.hpp"
#include "Events/EventBus.hpp"
#include "Events/Events.hpp"


namespace gestalt::application {

  AnimationSystem::AnimationSystem(Repository& repository, EventBus& event_bus)
      : repository_(repository), event_bus_(event_bus) {}

  glm::vec3 AnimationSystem::update_translation(const Entity& entity,
                                                AnimationChannel<glm::vec3>& translation_channel,
                                                bool loop) const {
    const auto& translation_keyframes = translation_channel.keyframes;
    float current_time = translation_channel.current_time;
    current_time += delta_time_;

    if (translation_keyframes.size() < 2) {
      return translation_keyframes[0].value;  // Not enough keyframes to interpolate
    }

    // Find the keyframe pair that surrounds the current time
    size_t start_index = 0;
    size_t end_index = 1;

    for (size_t i = 0; i < translation_keyframes.size() - 1; ++i) {
      if (current_time >= translation_keyframes[i].time
          && current_time <= translation_keyframes[i + 1].time) {
        start_index = i;
        end_index = i + 1;
        break;
      }
    }

    // Get the two keyframes
    auto& start_keyframe = translation_keyframes[start_index];
    auto& end_keyframe = translation_keyframes[end_index];

    // Compute the interpolation factor (t) between the start and end keyframes
    const float32 t
        = (current_time - start_keyframe.time) / (end_keyframe.time - start_keyframe.time);

    // Linearly interpolate the translation between the two keyframes
    const glm::vec3 interpolated_translation = mix(start_keyframe.value, end_keyframe.value, t);

    if (loop && current_time > translation_keyframes.back().time) {
      current_time = 0.0f;  // Reset for looping
    }
    translation_channel.current_time = current_time;

    return interpolated_translation;
  }

  glm::quat AnimationSystem::update_rotation(const Entity& entity,
                                             AnimationChannel<glm::quat>& rotation_channel,
                                             bool loop) const {
    const auto& rotation_keyframes = rotation_channel.keyframes;
    float current_time = rotation_channel.current_time;
    current_time += delta_time_;

    if (rotation_keyframes.size() < 2) {
      return rotation_keyframes[0].value;  // Not enough keyframes to interpolate
    }

    // Find the keyframe pair that surrounds the current time
    size_t start_index = 0;
    size_t end_index = 1;

    for (size_t i = 0; i < rotation_keyframes.size() - 1; ++i) {
      if (current_time >= rotation_keyframes[i].time
          && current_time <= rotation_keyframes[i + 1].time) {
        start_index = i;
        end_index = i + 1;
        break;
      }
    }

    // Get the two keyframes
    auto& start_keyframe = rotation_keyframes[start_index];
    auto& end_keyframe = rotation_keyframes[end_index];

    // Compute the interpolation factor (t) between the start and end keyframes
    const float32 t
        = (current_time - start_keyframe.time) / (end_keyframe.time - start_keyframe.time);

    // Linearly interpolate the translation between the two keyframes
    const glm::quat interpolated_rotation = glm::mix(start_keyframe.value, end_keyframe.value, t);

    if (loop && current_time > rotation_keyframes.back().time) {
      current_time = 0.0f;  // Reset for looping
    }
    rotation_channel.current_time = current_time;

    return glm::normalize(interpolated_rotation);
  }

  void AnimationSystem::update(const float delta_time) {
    delta_time_ = delta_time;
    for (auto [entity, animation_component] : repository_.animation_components.snapshot()) {
      const auto new_translation = update_translation(
          entity, animation_component.translation_channel, animation_component.loop);
      const auto new_rotation
          = update_rotation(entity, animation_component.rotation_channel, animation_component.loop);
      event_bus_.emit<TranslateEntityEvent>(TranslateEntityEvent{entity, new_translation});
      event_bus_.emit<RotateEntityEvent>(RotateEntityEvent{entity, new_rotation});

    }
  }

}  // namespace gestalt::application