#include "PhysicEngine.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

#include <glm/gtx/quaternion.hpp>

#include <cstdarg>
#include <iostream>
#include <thread>

#include "PhysicUtil.hpp"
#include "Components/PhysicsComponent.hpp"

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
    virtual bool ShouldCollide(JPH::ObjectLayer inObject1,
                               JPH::ObjectLayer inObject2) const override {
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

    virtual JPH::uint GetNumBroadPhaseLayers() const override {
      return BroadPhaseLayers::NUM_LAYERS;
    }

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
    virtual bool ShouldCollide(JPH::ObjectLayer inLayer1,
                               JPH::BroadPhaseLayer inLayer2) const override {
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
    virtual JPH::ValidateResult OnContactValidate(
        const JPH::Body &inBody1, const JPH::Body &inBody2, JPH::RVec3Arg inBaseOffset,
        const JPH::CollideShapeResult &inCollisionResult) override {
      //std::cout << "Contact validate callback" << std::endl;

      // Allows you to ignore a contact before it is created (using layers to not make objects
      // collide is cheaper!)
      return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    virtual void OnContactAdded(const JPH::Body &inBody1, const JPH::Body &inBody2,
                                const JPH::ContactManifold &inManifold,
                                JPH::ContactSettings &ioSettings) override {
      //std::cout << "A contact was added" << std::endl;
    }

    virtual void OnContactPersisted(const JPH::Body &inBody1, const JPH::Body &inBody2,
                                    const JPH::ContactManifold &inManifold,
                                    JPH::ContactSettings &ioSettings) override {
      //std::cout << "A contact was persisted" << std::endl;
    }

    virtual void OnContactRemoved(const JPH::SubShapeIDPair &inSubShapePair) override {
      //std::cout << "A contact was removed" << std::endl;
    }
  };

  // An example activation listener
  class MyBodyActivationListener : public JPH::BodyActivationListener {
  public:
    virtual void OnBodyActivated(const JPH::BodyID &inBodyID, uint64 inBodyUserData) override {
      //std::cout << "A body got activated" << std::endl;
    }

    virtual void OnBodyDeactivated(const JPH::BodyID &inBodyID, uint64 inBodyUserData) override {
      //std::cout << "A body went to sleep" << std::endl;
    }
  };

  void PhysicEngine::init() {
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
    temp_allocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);  // 10 MB
    job_system = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
                                              std::thread::hardware_concurrency() - 1);
    const JPH::uint cMaxBodies = 65536;
    const JPH::uint cNumBodyMutexes = 0;  // default
    const JPH::uint cMaxBodyPairs = 65536;
    const JPH::uint cMaxContactConstraints = 10240;

    broad_phase_layer_interface = new BPLayerInterfaceImpl();
    object_vs_broadphase_layer_filter = new ObjectVsBroadPhaseLayerFilterImpl();
    object_vs_object_layer_filter = new ObjectLayerPairFilterImpl();

    physics_system = new JPH::PhysicsSystem();
    physics_system->Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints,
                         *broad_phase_layer_interface, *object_vs_broadphase_layer_filter,
                         *object_vs_object_layer_filter);

    body_activation_listener = new MyBodyActivationListener();
    physics_system->SetBodyActivationListener(body_activation_listener);

    contact_listener = new MyContactListener();
    physics_system->SetContactListener(contact_listener);

    body_interface = &physics_system->GetBodyInterfaceNoLock();

    // Optional step: Before starting the physics simulation you can optimize the broad phase. This
    // improves collision detection performance (it's pointless here because we only have 2 bodies).
    // You should definitely not call this every frame or when e.g. streaming in a new level section
    // as it is an expensive operation. Instead insert all new objects in batches instead of 1 at a
    // time to keep the broad phase efficient.
    physics_system->OptimizeBroadPhase();
  }

  JPH::Body* PhysicEngine::create_body(PhysicsComponent &physics_component,
                                        const glm::vec3 &position,
                                        const glm::quat &orientation) const {
    JPH::EMotionType motion_type;
    JPH::ObjectLayer layer;

    if (physics_component.body_type == DYNAMIC) {
      motion_type = JPH::EMotionType::Dynamic;
      layer = Layers::MOVING;
    } else if (physics_component.body_type == STATIC) {
      motion_type = JPH::EMotionType::Static;
      layer = Layers::NON_MOVING;
    }

    if (physics_component.collider_type == BOX) {
      const auto &[bounds] = std::get<BoxCollider>(physics_component.collider);
      physics_component.shape = new JPH::BoxShape(to(bounds * 0.5f));
    } else if (physics_component.collider_type == SPHERE) {
      const auto &[radius] = std::get<SphereCollider>(physics_component.collider);
      physics_component.shape = new JPH::SphereShape(radius);
    } else if (physics_component.collider_type == CAPSULE) {
      const auto &[radius, height] = std::get<CapsuleCollider>(physics_component.collider);
      physics_component.shape = new JPH::CapsuleShape(height * 0.5f, radius);
    } else {
      physics_component.shape = new JPH::MeshShape();
    }

    auto body_settings = JPH::BodyCreationSettings(physics_component.shape, to(position),
                                                         to(orientation), motion_type, layer);
    if (physics_component.collider_type == CAPSULE) {
      // assumes player is capsule
      //body_settings.mLinearDamping = 0.1f;
      //body_settings.mAngularDamping = 0.1f;
      body_settings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY | JPH::EAllowedDOFs::TranslationZ | JPH::EAllowedDOFs::RotationY;
      body_settings.mFriction = 0.1f;
      body_settings.mRestitution = 0.1f;
      body_settings.mAllowSleeping = false;
    }

    JPH::Body* body = body_interface->CreateBody(body_settings);
    body_interface->AddBody(body->GetID(), JPH::EActivation::Activate);

    return body;
  }

  void PhysicEngine::step_simulation(const float delta_time) const {
    constexpr float fixed_time_step = 1.0f / 60.0f;  // 60 updates per second
    int num_steps = static_cast<int>(std::floor(delta_time / fixed_time_step));

    // Update the physics system in fixed time steps
    for (int i = 0; i < num_steps; ++i) {
      physics_system->Update(fixed_time_step, 1, temp_allocator, job_system);
    }

    // Handle the remaining time
    float remaining_time = delta_time - num_steps * fixed_time_step;
    if (remaining_time > 0.0f) {
      physics_system->Update(remaining_time, 1, temp_allocator, job_system);
    }
  }

  void PhysicEngine::get_body_transform(const JPH::Body* body, glm::vec3 &out_position,
                                        glm::quat &out_orientation) {
    const JPH::RVec3 position = body->GetCenterOfMassPosition();
    const JPH::Quat orientation = body->GetRotation();
    out_orientation = to(orientation);
    out_position = to(position);
  }

  void PhysicEngine::apply_force(JPH::Body *body, const glm::vec3 &force) {
    body->AddForce(to(force));
  }

  void PhysicEngine::apply_velocity(JPH::Body *body, const glm::vec3 &velocity) {
    body->SetLinearVelocity(to(velocity));
  }

  void PhysicEngine::set_rotation(const JPH::Body *body, const glm::quat &orientation) const {
    body_interface->SetRotation(body->GetID(), to(orientation), JPH::EActivation::Activate);
  }

  void PhysicEngine::cleanup() const {
    // body_interface->RemoveBody(JPH::BodyID(sphere_id));

    // body_interface->DestroyBody(JPH::BodyID(sphere_id));

    JPH::UnregisterTypes();

    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
  }

}  // namespace gestalt::application