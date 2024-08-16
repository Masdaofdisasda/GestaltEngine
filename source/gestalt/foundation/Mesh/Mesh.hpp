#pragma once
#include <string>
#include <vector>

#include "MeshSurface.hpp"
#include "BoundingSphere.hpp"

namespace gestalt::foundation {

  /**
     * \brief A Mesh holds all the data for a single renderable "object" in the scene. It has one or more MeshSurfaces
     * that are rendered together. The Mesh function like a node in the scene graph, because moving the mesh moves all
     * MeshSurfaces together. A MeshSurface references the geometry and the material for a single renderable part of the Mesh.
     */
    struct Mesh {
      std::string name;
      std::vector<MeshSurface> surfaces;
      BoundingSphere local_bounds;
    };
}  // namespace gestalt