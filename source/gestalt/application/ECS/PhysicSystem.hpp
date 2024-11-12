#pragma once

#include "PhysicEngine.hpp"
#include "Repository.hpp"
#include "BaseSystem.hpp"

namespace JPH {
  class Body;
  class BodyID;
  class JobSystemThreadPool;
  class TempAllocatorImpl;
  class BodyInterface;
  class PhysicsSystem;
}

namespace gestalt::foundation {
  struct UserInput;
}

namespace gestalt::application {

    class PhysicSystem final : public BaseSystem, public NonCopyable<PhysicSystem> {
      std::unique_ptr<PhysicEngine> physic_engine_;
      Entity player_ = invalid_entity;
    public:
      void prepare() override;
      void move_player(float delta_time, const UserInput& movement) const;
      void update(float delta_time, const UserInput& movement, float aspect) override;
      void cleanup() override;
    };

}  // namespace gestalt