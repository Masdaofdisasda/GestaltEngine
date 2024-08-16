#pragma once
#include <string>
#include <vector>

#include "Component.hpp"
#include "Entity.hpp"
#include "Mesh/AABB.hpp"

namespace gestalt::foundation {

    struct NodeComponent :Component {
      std::string name;
      Entity parent = invalid_entity;
      std::vector<Entity> children;
      AABB bounds;
      bool visible = true;
    };

}  // namespace gestalt