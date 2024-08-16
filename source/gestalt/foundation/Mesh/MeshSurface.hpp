#pragma once
#include "common.hpp"
#include "Components/Entity.hpp"
#include "BoundingSphere.hpp"

namespace gestalt::foundation {

    
  /**
   * \brief A MeshSurface is a single renderable part of a Mesh. It references the geometry and the material for a single
   * Mesh. The MeshSurface is rendered as a single draw call, and the geometry is split into Meshlets.
   */
  struct MeshSurface {
      uint32 meshlet_offset;
      uint32 meshlet_count;

      uint32 vertex_count;
      uint32 index_count;
      uint32 first_index;
      uint32 vertex_offset;
      BoundingSphere local_bounds;

      size_t material = default_material;
      size_t mesh_draw = no_component;
    };
}  // namespace gestalt