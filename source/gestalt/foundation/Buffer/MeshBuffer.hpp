#pragma once
#include <array>
#include <memory>
#include <vector>

#include "BufferCollection.hpp"
#include "EngineConfiguration.hpp"
#include "common.hpp"
#include "Descriptor/DescriptorBuffer.hpp"

namespace gestalt::foundation {

  struct MeshBuffers final : BufferCollection {
    std::shared_ptr<BufferInstance> index_buffer;                // regular index buffer
    std::shared_ptr<BufferInstance> vertex_position_buffer;  // only vertex positions
    std::shared_ptr<BufferInstance> vertex_data_buffer;          // normals, tangents, uvs

  std::shared_ptr<BufferInstance> meshlet_buffer;  // meshlets
    std::shared_ptr<BufferInstance> meshlet_vertices;
  std::shared_ptr<BufferInstance> meshlet_triangles;  // meshlet indices
    std::array<std::shared_ptr<BufferInstance>, getFramesInFlight()> meshlet_task_commands_buffer;
  std::array<std::shared_ptr<BufferInstance>, getFramesInFlight()>
      mesh_draw_buffer;  // TRS, material id
    std::array<std::shared_ptr<BufferInstance>, getFramesInFlight()>
      draw_count_buffer;  // number of draws

  
    std::shared_ptr<BufferInstance> index_buffer_instance;
  std::shared_ptr<BufferInstance> vertex_position_buffer_instance;
    std::shared_ptr<BufferInstance> vertex_data_buffer_instance;

  std::shared_ptr<BufferInstance> meshlet_buffer_instance;
    std::shared_ptr<BufferInstance> meshlet_vertices_instance;
  std::shared_ptr<BufferInstance> meshlet_triangles_instance;
    std::shared_ptr<BufferInstance> meshlet_task_commands_buffer_instance;
  std::shared_ptr<BufferInstance> mesh_draw_buffer_instance;
    std::shared_ptr<BufferInstance> command_count_buffer_instance;
    std::shared_ptr<BufferInstance> group_count_buffer_instance;


  std::array<std::shared_ptr<DescriptorBuffer>, getFramesInFlight()> descriptor_buffers;

  std::vector<std::shared_ptr<BufferInstance>> get_buffers(int16 frame_index) const override {
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