#pragma once

#include "Resources/AccelerationStructure.hpp"
#include <memory>

namespace gestalt::foundation {

  struct MeshBuffers final {

    std::shared_ptr<BufferInstance> index_buffer;          // regular index buffer
  std::shared_ptr<BufferInstance> vertex_position_buffer; // only vertex positions
    std::shared_ptr<BufferInstance> vertex_data_buffer; // normals, tangents, uvs

  std::shared_ptr<BufferInstance> meshlet_buffer;  // meshlets
    std::shared_ptr<BufferInstance> meshlet_vertices;
  std::shared_ptr<BufferInstance> meshlet_triangles; // meshlet indices
    std::shared_ptr<BufferInstance> meshlet_task_commands_buffer;
  std::shared_ptr<BufferInstance> mesh_draw_buffer; // TRS, material id
    std::shared_ptr<BufferInstance> command_count_buffer; // number of draws
    std::shared_ptr<BufferInstance> group_count_buffer;
    std::shared_ptr<BufferInstance> luminance_histogram_buffer;

    std::shared_ptr<BufferInstance> bottom_level_acceleration_structure_buffer;
    std::vector<std::shared_ptr<AccelerationStructure>> bottom_level_acceleration_structures;
  };
}  // namespace gestalt