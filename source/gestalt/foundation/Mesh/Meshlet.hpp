#pragma once

#include "common.hpp"

#include <glm/gtx/transform.hpp>

namespace gestalt::foundation {

    struct alignas(16) Meshlet {
      glm::vec3 center;
      float32 radius;

      int8 cone_axis[3];
      int8 cone_cutoff;

      uint32 vertex_offset; // offset into the global vertex buffer for this meshlet
      uint32 index_offset; // offset into the global index buffer for this meshlet
      uint32 mesh_draw_index;  // index into the mesh or MeshDraw that this meshlet belongs to
      uint8 vertex_count;
      uint8 triangle_count;
    };
}  // namespace gestalt