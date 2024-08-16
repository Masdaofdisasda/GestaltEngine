#pragma once

#include "common.hpp"

#include <glm/gtx/transform.hpp>

namespace gestalt::foundation {

  /**
   * \brief A MeshDraw contains all the data needed to draw a single MeshSurface. It has
   * transformation data for world space transformation, bounding data for culling, and references
   * to the vertex and index data for the MeshSurface.
   */
  struct MeshDraw {
    // TRS
    glm::vec3 position;
    float32 scale;
    glm::quat orientation;

    // Bounding sphere
    // Meshlet data
    glm::vec3 center;
    float32 radius;
    uint32 meshlet_offset;
    uint32 meshlet_count;

    // Vertex data , maybe unneeded?
    uint32 vertex_count;
    uint32 index_count;
    uint32 first_index;
    uint32 vertex_offset;

    // Material
    uint32 materialIndex;
    int32 pad;
  };
}  // namespace gestalt