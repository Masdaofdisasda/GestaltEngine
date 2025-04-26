#pragma once

#include "PhysicEngine.hpp"
#include "Repository.hpp"

namespace gestalt::application {
  class EventBus;
}

namespace JPH {
  class Body;
  class BodyID;
  class JobSystemThreadPool;
  class TempAllocatorImpl;
  class BodyInterface;
  class PhysicsSystem;
}

namespace gestalt::foundation {
  struct FrameProvider;
  class IResourceAllocator;
  struct UserInput;
}

namespace gestalt::application {

    class PhysicSystem final {
    IGpu& gpu_;
    IResourceAllocator& resource_allocator_;
    Repository& repository_;
    FrameProvider& frame_;
    EventBus& event_bus_;
      std::unique_ptr<PhysicEngine> physic_engine_;
      Entity player_ = invalid_entity;
    public:
      PhysicSystem(IGpu& gpu, IResourceAllocator& resource_allocator, Repository& repository,
                   FrameProvider& frame, EventBus& event_bus);
      ~PhysicSystem() = default;

      PhysicSystem(const PhysicSystem&) = delete;
      PhysicSystem& operator=(const PhysicSystem&) = delete;

      PhysicSystem(PhysicSystem&&) = delete;
      PhysicSystem& operator=(PhysicSystem&&) = delete;

      void move_player(float delta_time, const UserInput& movement) const;
      void update(float delta_time, const UserInput& movement) const;
    };

}  // namespace gestalt