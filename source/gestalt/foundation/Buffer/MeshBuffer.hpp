#pragma once
#include <array>
#include <memory>
#include <vector>

#include "BufferCollection.hpp"
#include "EngineConfiguration.hpp"
#include "common.hpp"
#include "Descriptor/DescriptorBuffer.hpp"
#include "Resources/AllocatedBuffer.hpp"

namespace gestalt::foundation {

  struct MeshBuffers final : BufferCollection {
  std::shared_ptr<AllocatedBuffer> index_buffer;            // regular index buffer
  std::shared_ptr<AllocatedBuffer> vertex_position_buffer;  // only vertex positions
  std::shared_ptr<AllocatedBuffer> vertex_data_buffer;      // normals, tangents, uvs

  std::shared_ptr<AllocatedBuffer> meshlet_buffer;  // meshlets
  std::shared_ptr<AllocatedBuffer> meshlet_vertices;
  std::shared_ptr<AllocatedBuffer> meshlet_triangles;  // meshlet indices
  std::array<std::shared_ptr<AllocatedBuffer>, getFramesInFlight()> meshlet_task_commands_buffer;
  std::array<std::shared_ptr<AllocatedBuffer>, getFramesInFlight()>
      mesh_draw_buffer;  // TRS, material id
  std::array<std::shared_ptr<AllocatedBuffer>, getFramesInFlight()>
      draw_count_buffer;  // number of draws

  std::array<std::shared_ptr<DescriptorBuffer>, getFramesInFlight()> descriptor_buffers;

  std::vector<std::shared_ptr<AllocatedBuffer>> get_buffers(int16 frame_index) const override {
    return {index_buffer,
            vertex_position_buffer,
            vertex_data_buffer,
            meshlet_buffer,
            meshlet_vertices,
            meshlet_triangles,
            meshlet_task_commands_buffer[frame_index],
            mesh_draw_buffer[frame_index],
            draw_count_buffer[frame_index]};
  }

  std::shared_ptr<DescriptorBuffer> get_descriptor_buffer(
      int16 frame_index) const override {
    return descriptor_buffers[frame_index];
  }
};
}  // namespace gestalt