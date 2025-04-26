#pragma once

#include <memory>

#include "common.hpp"

namespace gestalt::foundation {
  struct FrameProvider;
  class Repository;
  class IResourceAllocator;
  class IGpu;
}

namespace gestalt::application {

    struct AudioEngine;

    class AudioSystem final {
    std::unique_ptr<AudioEngine> audio_engine_; 
    uint32 current_frame_ = 0;

  public:
    AudioSystem();
    ~AudioSystem();

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    AudioSystem(AudioSystem&&) = delete;
    AudioSystem& operator=(AudioSystem&&) = delete;

    void update();

    };

}  // namespace gestalt