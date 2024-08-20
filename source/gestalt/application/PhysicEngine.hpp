#pragma once
#include <glm/gtx/transform.hpp>

#include "common.hpp"

namespace gestalt::foundation {
  struct PhysicsComponent;
}

namespace JPH {
  class TempAllocatorImpl;
  class JobSystemThreadPool;
  class PhysicsSystem;
  class BodyInterface;
  class Body;
  class BodyID;
}

namespace gestalt::application {
  class BPLayerInterfaceImpl;
  class ObjectVsBroadPhaseLayerFilterImpl;
  class ObjectLayerPairFilterImpl;
  class MyBodyActivationListener;
  class MyContactListener;

  class PhysicEngine {
    BPLayerInterfaceImpl* broad_phase_layer_interface;
    ObjectVsBroadPhaseLayerFilterImpl* object_vs_broadphase_layer_filter;
    ObjectLayerPairFilterImpl* object_vs_object_layer_filter;
    MyBodyActivationListener* body_activation_listener;
    MyContactListener* contact_listener;
    JPH::TempAllocatorImpl* temp_allocator;
    JPH::JobSystemThreadPool* job_system;
    JPH::PhysicsSystem* physics_system;
    JPH::BodyInterface* body_interface;

    JPH::Body* floor = nullptr;

    public:
    void init();

    JPH::Body* create_body(PhysicsComponent& physics_component,
                            const glm::vec3& position, const glm::quat& orientation) const;
    void remove_body(JPH::BodyID body_id);

    void step_simulation(float delta_time) const;
    static void get_body_transform(const JPH::Body* body, glm::vec3& out_position,
                                   glm::quat& out_orientation);
    static void apply_force(JPH::Body* body, const glm::vec3& force);
    static void apply_velocity(JPH::Body* body, const glm::vec3& velocity);

    void set_gravity_enabled(JPH::BodyID body_id, bool enabled);

    void cleanup() const;
  };

}  // namespace gestalt