#pragma once

#include <Jolt/Jolt.h>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

inline JPH::RVec3 to(const glm::vec3 &v) { return {v.x, v.y, v.z}; }

inline JPH::Quat to(const glm::quat &q) { return {q.x, q.y, q.z, q.w}; }

inline glm::vec3 to(const JPH::RVec3 &v) { return {v.GetX(), v.GetY(), v.GetZ()}; }

inline glm::quat to(const JPH::Quat &q) { return {q.GetW(), q.GetX(), q.GetY(), q.GetZ()}; }