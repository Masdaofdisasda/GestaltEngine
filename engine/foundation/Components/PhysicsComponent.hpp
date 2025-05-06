#pragma once
#include <variant>
#include "Component.hpp"
#include "common.hpp"

namespace JPH {
  class Shape;
  class Body;
}  // namespace JPH

namespace gestalt::foundation {

  enum BodyType { STATIC, DYNAMIC };

  enum ColliderType { BOX, SPHERE, CAPSULE, MESH, TERRAIN };

  struct BoxCollider {
    glm::vec3 bounds;
  };

  struct SphereCollider {
    float32 radius = 1.0f;
  };

  struct CapsuleCollider {
    float32 radius = 1.0f;
    float32 height = 1.0f;
  };

  struct MeshCollider {
    std::string mesh_path;
  };

  struct TerrainCollider {
    std::string heightmap_path;
    float32 height_scale = 1.0f;
    float32 xz_scale = 1.0f;
  };

  using Collider = std::variant<BoxCollider, SphereCollider, CapsuleCollider, MeshCollider, TerrainCollider>;

  class PhysicsComponent : Component {
    BodyType body_type_;          // Indicates whether the body is static, dynamic, or kinematic
    ColliderType collider_type_;  // Shape of the collider
    Collider collider_;           // Collider data

    // uint32 body_id = invalid_entity;   // ID of the corresponding physics body in Jolt
    JPH::Body* body_ = nullptr;    // Pointer to the physics body
    JPH::Shape* shape_ = nullptr;  // Pointer to the shape of the collider

  public:
    PhysicsComponent(const BodyType type, SphereCollider collider)
        : body_type_(type), collider_type_(SPHERE), collider_(collider) {}

    PhysicsComponent(const BodyType type, BoxCollider collider)
        : body_type_(type), collider_type_(BOX), collider_(collider) {}

    PhysicsComponent(const BodyType type, CapsuleCollider collider)
        : body_type_(type), collider_type_(CAPSULE), collider_(collider) {}

    bool is_body_type(const BodyType type) const {
      return body_type_ == type;
    }
    bool is_collider_type(const ColliderType type) const { return collider_type_ == type; }
    const Collider& collider() const { return collider_; }
    JPH::Body* body() const { return body_; }
    void set_body(JPH::Body* body) { body_ = body; }
    JPH::Shape* shape() const { return shape_; }
    void set_shape(JPH::Shape* shape) { shape_ = shape; }
  };


}  // namespace gestalt