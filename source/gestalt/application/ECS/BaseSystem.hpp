#pragma once

#include "Repository.hpp"


namespace gestalt::foundation {
  struct UserInput;
  class IResourceManager;
  class IDescriptorLayoutBuilder;
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
                IResourceManager* resource_manager,
                IDescriptorLayoutBuilder* builder,
                Repository* repository,
                FrameProvider* frame) {
        gpu_ = gpu;
        resource_manager_ = resource_manager;
        descriptor_layout_builder_ = builder;
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
      IResourceManager* resource_manager_ = nullptr;
      IDescriptorLayoutBuilder* descriptor_layout_builder_ = nullptr;
      Repository* repository_ = nullptr;
      FrameProvider* frame_ = nullptr;
    };

}  // namespace gestalt