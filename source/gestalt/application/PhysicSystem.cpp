﻿#include "SceneSystem.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

#include <iostream>
#include <cstdarg>
#include <thread>

JPH_SUPPRESS_WARNINGS

namespace gestalt::application {
  namespace Layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
  };  // namespace Layers

/// Class that determines if two object layers can collide
  class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
  public:
    virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
      switch (inObject1) {
        case Layers::NON_MOVING:
          return inObject2 == Layers::MOVING;  // Non moving only collides with moving
        case Layers::MOVING:
          return true;  // Moving collides with everything
        default:
          return false;
      }
    }
  };

  // Each broadphase layer results in a separate bounding volume tree in the broad phase. You at
  // least want to have a layer for non-moving and moving objects to avoid having to update a tree
  // full of static objects every frame. You can have a 1-on-1 mapping between object layers and
  // broadphase layers (like in this case) but if you have many object layers you'll be creating
  // many broad phase trees, which is not efficient. If you want to fine tune your broadphase layers
  // define JPH_TRACK_BROADPHASE_STATS and look at the stats reported on the TTY.
  namespace BroadPhaseLayers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr JPH::uint NUM_LAYERS(2);
  };  // namespace BroadPhaseLayers

  // BroadPhaseLayerInterface implementation
  // This defines a mapping between object and broadphase layers.
  class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
  public:
    BPLayerInterfaceImpl() {
      // Create a mapping table from object to broad phase layer
      mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
      mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }

    virtual JPH::uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }

    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
      JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
      return mObjectToBroadPhase[inLayer];
    }

  private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
  };

  /// Class that determines if an object layer can collide with a broadphase layer
  class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
  public:
    virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
      switch (inLayer1) {
        case Layers::NON_MOVING:
          return inLayer2 == BroadPhaseLayers::MOVING;
        case Layers::MOVING:
          return true;
        default:
          JPH_ASSERT(false);
          return false;
      }
    }
  };

  // An example contact listener
  class MyContactListener : public JPH::ContactListener {
  public:
    // See: ContactListener
    virtual JPH::ValidateResult OnContactValidate(const JPH::Body &inBody1, const JPH::Body &inBody2,
                                                  JPH::RVec3Arg inBaseOffset,
                                                  const JPH::CollideShapeResult &inCollisionResult) override {
      std::cout << "Contact validate callback" << std::endl;

      // Allows you to ignore a contact before it is created (using layers to not make objects
      // collide is cheaper!)
      return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    virtual void OnContactAdded(const JPH::Body &inBody1, const JPH::Body &inBody2,
                                const JPH::ContactManifold &inManifold,
                                JPH::ContactSettings &ioSettings) override {
      std::cout << "A contact was added" << std::endl;
    }

    virtual void OnContactPersisted(const JPH::Body &inBody1, const JPH::Body &inBody2,
                                    const JPH::ContactManifold &inManifold,
                                    JPH::ContactSettings &ioSettings) override {
      std::cout << "A contact was persisted" << std::endl;
    }

    virtual void OnContactRemoved(const JPH::SubShapeIDPair &inSubShapePair) override {
      std::cout << "A contact was removed" << std::endl;
    }
  };

  // An example activation listener
  class MyBodyActivationListener : public JPH::BodyActivationListener {
  public:
    virtual void OnBodyActivated(const JPH::BodyID &inBodyID, uint64 inBodyUserData) override {
      std::cout << "A body got activated" << std::endl;
    }

    virtual void OnBodyDeactivated(const JPH::BodyID &inBodyID, uint64 inBodyUserData) override {
      std::cout << "A body went to sleep" << std::endl;
    }
  };

  void PhysicSystem::prepare() {
    // Register allocation hook. In this example we'll just let Jolt use malloc / free but you can
    // override these if you want (see Memory.h). This needs to be done before any other Jolt
    // function is called.
    JPH::RegisterDefaultAllocator();

    // Create a factory, this class is responsible for creating instances of classes based on their
    // name or hash and is mainly used for deserialization of saved data. It is not directly used in
    // this example but still required.
    JPH::Factory::sInstance = new JPH::Factory();

    // Register all physics types with the factory and install their collision handlers with the
    // CollisionDispatch class. If you have your own custom shape types you probably need to
    // register their handlers with the CollisionDispatch before calling this function. If you
    // implement your own default material (PhysicsMaterial::sDefault) make sure to initialize it
    // before this function or else this function will create one for you.
    JPH::RegisterTypes();

    // We need a temp allocator for temporary allocations during the physics update. We're
    // pre-allocating 10 MB to avoid having to do allocations during the physics update.
    // B.t.w. 10 MB is way too much for this example but it is a typical value you can use.
    // If you don't want to pre-allocate you can also use TempAllocatorMalloc to fall back to
    // malloc / free.
    JPH::TempAllocatorImpl temp_allocator(10 * 1024 * 1024);

    // We need a job system that will execute physics jobs on multiple threads. Typically
    // you would implement the JobSystem interface yourself and let Jolt Physics run on top
    // of your own job scheduler. JobSystemThreadPool is an example implementation.
    JPH::JobSystemThreadPool job_system(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
                                        std::thread::hardware_concurrency() - 1);

    // This is the max amount of rigid bodies that you can add to the physics system. If you try to
    // add more you'll get an error. Note: This value is low because this is a simple test. For a
    // real project use something in the order of 65536.
    const JPH::uint cMaxBodies = 1024;

    // This determines how many mutexes to allocate to protect rigid bodies from concurrent access.
    // Set it to 0 for the default settings.
    const JPH::uint cNumBodyMutexes = 0;

    // This is the max amount of body pairs that can be queued at any time (the broad phase will
    // detect overlapping body pairs based on their bounding boxes and will insert them into a queue
    // for the narrowphase). If you make this buffer too small the queue will fill up and the broad
    // phase jobs will start to do narrow phase work. This is slightly less efficient. Note: This
    // value is low because this is a simple test. For a real project use something in the order of
    // 65536.
    const JPH::uint cMaxBodyPairs = 1024;

    // This is the maximum size of the contact constraint buffer. If more contacts (collisions
    // between bodies) are detected than this number then these contacts will be ignored and bodies
    // will start interpenetrating / fall through the world. Note: This value is low because this is
    // a simple test. For a real project use something in the order of 10240.
    const JPH::uint cMaxContactConstraints = 1024;

    // Create mapping table from object layer to broadphase layer
    // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance
    // needs to stay alive!
    BPLayerInterfaceImpl broad_phase_layer_interface;

    // Create class that filters object vs broadphase layers
    // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance
    // needs to stay alive!
    ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_layer_filter;

    // Create class that filters object vs object layers
    // Note: As this is an interface, PhysicsSystem will take a reference to this so this instance
    // needs to stay alive!
    ObjectLayerPairFilterImpl object_vs_object_layer_filter;

    // Now we can create the actual physics system.
    JPH::PhysicsSystem physics_system;
    physics_system.Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints,
                        broad_phase_layer_interface, object_vs_broadphase_layer_filter,
                        object_vs_object_layer_filter);

    // A body activation listener gets notified when bodies activate and go to sleep
    // Note that this is called from a job so whatever you do here needs to be thread safe.
    // Registering one is entirely optional.
    MyBodyActivationListener body_activation_listener;
    physics_system.SetBodyActivationListener(&body_activation_listener);

    // A contact listener gets notified when bodies (are about to) collide, and when they separate
    // again. Note that this is called from a job so whatever you do here needs to be thread safe.
    // Registering one is entirely optional.
    MyContactListener contact_listener;
    physics_system.SetContactListener(&contact_listener);

    // The main way to interact with the bodies in the physics system is through the body interface.
    // There is a locking and a non-locking variant of this. We're going to use the locking version
    // (even though we're not planning to access bodies from multiple threads)
    JPH::BodyInterface &body_interface = physics_system.GetBodyInterface();

    // Next we can create a rigid body to serve as the floor, we make a large box
    // Create the settings for the collision volume (the shape).
    // Note that for simple shapes (like boxes) you can also directly construct a BoxShape.
    JPH::BoxShapeSettings floor_shape_settings(JPH::Vec3(100.0f, 1.0f, 100.0f));
    floor_shape_settings.SetEmbedded();  // A ref counted object on the stack (base class RefTarget)
                                         // should be marked as such to prevent it from being freed
                                         // when its reference count goes to 0.

    // Create the shape
    JPH::ShapeSettings::ShapeResult floor_shape_result = floor_shape_settings.Create();
    JPH::ShapeRefC floor_shape
        = floor_shape_result.Get();  // We don't expect an error here, but you can check
                                     // floor_shape_result for HasError() / GetError()

    // Create the settings for the body itself. Note that here you can also set other properties
    // like the restitution / friction.
    JPH::BodyCreationSettings floor_settings(floor_shape, JPH::RVec3(0.0f, -1.0f, 0.0f), JPH::Quat::sIdentity(),
                                             JPH::EMotionType::Static, Layers::NON_MOVING);

    // Create the actual rigid body
    JPH::Body *floor = body_interface.CreateBody(
        floor_settings);  // Note that if we run out of bodies this can return nullptr

    // Add it to the world
    body_interface.AddBody(floor->GetID(), JPH::EActivation::DontActivate);

    // Now create a dynamic body to bounce on the floor
    // Note that this uses the shorthand version of creating and adding a body to the world
    JPH::BodyCreationSettings sphere_settings(new JPH::SphereShape(0.5f), JPH::RVec3(0.0f, 2.0f, 0.0f),
                                              JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, Layers::MOVING);
    JPH::BodyID sphere_id = body_interface.CreateAndAddBody(sphere_settings, JPH::EActivation::Activate);

    // Now you can interact with the dynamic body, in this case we're going to give it a velocity.
    // (note that if we had used CreateBody then we could have set the velocity straight on the body
    // before adding it to the physics system)
    body_interface.SetLinearVelocity(sphere_id, JPH::Vec3(0.0f, -5.0f, 0.0f));

    // We simulate the physics world in discrete time steps. 60 Hz is a good rate to update the
    // physics system.
    const float cDeltaTime = 1.0f / 60.0f;

    // Optional step: Before starting the physics simulation you can optimize the broad phase. This
    // improves collision detection performance (it's pointless here because we only have 2 bodies).
    // You should definitely not call this every frame or when e.g. streaming in a new level section
    // as it is an expensive operation. Instead insert all new objects in batches instead of 1 at a
    // time to keep the broad phase efficient.
    physics_system.OptimizeBroadPhase();
  }

  void PhysicSystem::update() {
    
  }

  void PhysicSystem::cleanup() {
    
  }

}  // namespace gestalt