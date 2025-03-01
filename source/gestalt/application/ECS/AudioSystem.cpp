#include "AudioSystem.hpp"

#include <soloud.h>
#include <soloud_wav.h>

namespace gestalt::application {

  struct AudioEngine {
    std::unique_ptr<SoLoud::Soloud> engine = std::make_unique<SoLoud::Soloud>();
    std::unique_ptr<SoLoud::Wav> sound_wave = std::make_unique<SoLoud::Wav>();

    AudioEngine() = default;

    ~AudioEngine() = default;
  };

  AudioSystem::AudioSystem() : audio_engine_(std::make_unique<AudioEngine>()) {
    audio_engine_->engine->init();
    // audio_engine_->sound_wave->load("../../assets/GestaltDemo.mp3");
    // audio_engine_->sound_wave->setLooping(true);
  }

  void AudioSystem::update() {
    current_frame_++;
    // audio_engine_->engine->play(*audio_engine_->sound_wave);
  }
  AudioSystem::~AudioSystem() {
    audio_engine_->engine->stopAll();
    audio_engine_->engine->deinit();
  }
}  // namespace gestalt::application