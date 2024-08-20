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

  enum class ColliderType { BOX, SPHERE, CAPSULE, MESH, TERRAIN };

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

  struct PhysicsComponent : Component {
    BodyType body_type;              // Indicates whether the body is static, dynamic, or kinematic
    ColliderType collider_type;      // Shape of the collider
    Collider collider;               // Collider data

    //uint32 body_id = invalid_entity;   // ID of the corresponding physics body in Jolt
    JPH::Body* body = nullptr;        // Pointer to the physics body
    JPH::Shape* shape = nullptr;      // Pointer to the shape of the collider

    PhysicsComponent(const BodyType type, SphereCollider collider)
        : body_type(type), collider_type(ColliderType::SPHERE), collider(collider) {}

    PhysicsComponent(const BodyType type, BoxCollider collider)
        : body_type(type), collider_type(ColliderType::BOX), collider(collider) {}
  };

}  // namespace gestalt