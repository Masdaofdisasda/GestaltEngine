﻿#pragma once

#include "Repository.hpp"


namespace gestalt::foundation {
  class IResourceAllocator;
  struct UserInput;
  class IGpu;
  struct FrameProvider;
}

namespace gestalt::application {
  class MyContactListener;
  class MyBodyActivationListener;
  class ObjectLayerPairFilterImpl;
  class ObjectVsBroadPhaseLayerFilterImpl;
  class BPLayerInterfaceImpl;

    class BaseSystem {
    public:
      void init(IGpu* gpu,
                IResourceAllocator* resource_allocator,
                Repository* repository,
                FrameProvider* frame) {
        gpu_ = gpu;
        resource_allocator_ = resource_allocator;
        repository_ = repository;
        frame_ = frame; 

        prepare();
      }
      virtual ~BaseSystem() = default;

      virtual void update(float delta_time, const UserInput& movement, float aspect) = 0;
      virtual void cleanup(){}

    protected:
      virtual void prepare(){}

      IGpu* gpu_ = nullptr;
      IResourceAllocator* resource_allocator_ = nullptr;
      Repository* repository_ = nullptr;
      FrameProvider* frame_ = nullptr;
    };

}  // namespace gestalt