#pragma once

#include "BaseSystem.hpp"
#include <memory>

namespace gestalt::application {

    struct AudioEngine;

    class AudioSystem final {
      IGpu& gpu_;
      IResourceAllocator& resource_allocator_;
      Repository& repository_;
      FrameProvider& frame_;
    std::unique_ptr<AudioEngine> audio_engine_; 
    uint32 current_frame_ = 0;

  public:
    AudioSystem(IGpu& gpu, IResourceAllocator& resource_allocator, Repository& repository,
                FrameProvider& frame);
    ~AudioSystem();

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    AudioSystem(AudioSystem&&) = delete;
    AudioSystem& operator=(AudioSystem&&) = delete;

    void update();

    };

}  // namespace gestalt