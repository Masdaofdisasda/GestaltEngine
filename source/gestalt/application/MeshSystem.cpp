#include "RenderConfig.hpp"
#include "SceneSystem.hpp"

namespace gestalt::application {

  void MeshSystem::prepare() {
    constexpr size_t initial_vertex_position_buffer_size
        = getMaxVertices() * sizeof(GpuVertexPosition);
    constexpr size_t initial_vertex_data_buffer_size = getMaxVertices() * sizeof(GpuVertexData);
    constexpr size_t initial_index_buffer_size = getMaxIndices() * sizeof(uint32);

    MeshBuffers mesh_buffers;

    descriptor_layout_builder_->clear();
    constexpr auto geometry_stages
        = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_COMPUTE_BIT;
    mesh_buffers.descriptor_layout
        = descriptor_layout_builder_
              ->add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, geometry_stages)
              .add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, geometry_stages)
              .add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, geometry_stages)
              .add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, geometry_stages)
              .add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, geometry_stages)
              .add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, geometry_stages)
              .add_binding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, geometry_stages)
              .add_binding(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, geometry_stages)
              .build(gpu_->getDevice());

    for (auto& descriptor_set : mesh_buffers.descriptor_sets) {
      descriptor_set = resource_manager_->get_descriptor_pool()->allocate(
          gpu_->getDevice(), mesh_buffers.descriptor_layout);
    }

    // Create initially empty vertex buffer
    mesh_buffers.vertex_position_buffer = resource_manager_->create_buffer(
        initial_vertex_position_buffer_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    mesh_buffers.vertex_data_buffer = resource_manager_->create_buffer(
        initial_vertex_data_buffer_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    // Create initially empty index buffer
    mesh_buffers.index_buffer = resource_manager_->create_buffer(
        initial_index_buffer_size,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    // Create meshlet buffers
    mesh_buffers.meshlet_buffer = resource_manager_->create_buffer(
        getMaxMeshlets() * sizeof(Meshlet),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    mesh_buffers.meshlet_vertices = resource_manager_->create_buffer(
        getMaxVertices() * sizeof(uint32),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    mesh_buffers.meshlet_triangles = resource_manager_->create_buffer(
        getMaxIndices() * sizeof(uint8),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    // Create mesh task commands and draw buffers
    for (int i = 0; i < getFramesInFlight(); i++) {
      mesh_buffers.meshlet_task_commands_buffer[i] = resource_manager_->create_buffer(
          getMaxMeshlets() * sizeof(MeshTaskCommand),
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
          VMA_MEMORY_USAGE_GPU_ONLY);
      mesh_buffers.mesh_draw_buffer[i] = resource_manager_->create_buffer(
          getMaxMeshes() * sizeof(MeshDraw), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
          VMA_MEMORY_USAGE_CPU_TO_GPU);
      mesh_buffers.draw_count_buffer[i] = resource_manager_->create_buffer(
          4 * sizeof(uint32),
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
              | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
          VMA_MEMORY_USAGE_GPU_ONLY);
    }

    repository_->register_buffer(mesh_buffers);

    for (int i = 0; i < getFramesInFlight(); i++) {
      writer_->clear();
      writer_->write_buffer(0, mesh_buffers.vertex_position_buffer.buffer,
                            initial_vertex_position_buffer_size, 0,
                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
      writer_->write_buffer(1, mesh_buffers.vertex_data_buffer.buffer,
                            initial_vertex_data_buffer_size, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
      writer_->write_buffer(2, mesh_buffers.meshlet_buffer.buffer,
                            getMaxMeshlets() * sizeof(Meshlet), 0,
                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
      writer_->write_buffer(3, mesh_buffers.meshlet_vertices.buffer,
                            getMaxVertices() * sizeof(uint32), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
      writer_->write_buffer(4, mesh_buffers.meshlet_triangles.buffer,
                            getMaxMeshlets() * sizeof(uint8), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
      writer_->write_buffer(5, mesh_buffers.meshlet_task_commands_buffer[i].buffer,
                            getMaxMeshlets() * sizeof(MeshTaskCommand), 0,
                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
      writer_->write_buffer(6, mesh_buffers.mesh_draw_buffer[i].buffer,
                            getMaxMeshes() * sizeof(MeshDraw), 0,
                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
      writer_->write_buffer(7, mesh_buffers.draw_count_buffer[i].buffer, 4 * sizeof(uint32), 0,
                                     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
      writer_->update_set(gpu_->getDevice(), mesh_buffers.descriptor_sets[i]);
    }
  }

  void MeshSystem::upload_mesh() {
    const std::span indices = repository_->indices.data();
    const std::span vertex_positions = repository_->vertex_positions.data();
    const std::span vertex_data = repository_->vertex_data.data();
    const std::span meshlets = repository_->meshlets.data();
    const std::span meshlet_vertices = repository_->meshlet_vertices.data();
    const std::span meshlet_triangles = repository_->meshlet_triangles.data();

    const size_t vertex_position_buffer_size = vertex_positions.size() * sizeof(GpuVertexPosition);
    const size_t vertex_data_buffer_size = vertex_data.size() * sizeof(GpuVertexData);
    const size_t index_buffer_size = indices.size() * sizeof(uint32_t);
    const size_t meshlet_buffer_size = meshlets.size() * sizeof(Meshlet);
    const size_t meshlet_vertices_size = meshlet_vertices.size() * sizeof(uint32);
    const size_t meshlet_triangles_size = meshlet_triangles.size() * sizeof(uint8);

    const auto& mesh_buffers = repository_->get_buffer<MeshBuffers>();

    if (mesh_buffers.vertex_position_buffer.info.size < vertex_position_buffer_size) {
      fmt::println("vertex_position_buffer size needs to be increased by {}",
                   vertex_position_buffer_size - mesh_buffers.vertex_position_buffer.info.size);
    }

    if (mesh_buffers.vertex_data_buffer.info.size < vertex_data_buffer_size) {
      fmt::println("vertex_data_buffer size needs to be increased by {}",
                   vertex_data_buffer_size - mesh_buffers.vertex_data_buffer.info.size);
    }

    if (mesh_buffers.index_buffer.info.size < index_buffer_size) {
      fmt::println("index_buffer size needs to be increased by {}",
                   index_buffer_size - mesh_buffers.index_buffer.info.size);
    }

    if (mesh_buffers.meshlet_buffer.info.size < meshlet_buffer_size) {
      fmt::println("meshlet_buffer size needs to be increased by {}",
                   meshlet_buffer_size - mesh_buffers.meshlet_buffer.info.size);
    }

    if (mesh_buffers.meshlet_vertices.info.size < meshlet_vertices_size) {
      fmt::println("meshlet_vertices size needs to be increased by {}",
                   meshlet_buffer_size - mesh_buffers.meshlet_vertices.info.size);
    }

    if (mesh_buffers.meshlet_triangles.info.size < meshlet_triangles_size) {
      fmt::println("meshlet_triangles size needs to be increased by {}",
                   meshlet_triangles_size - mesh_buffers.meshlet_triangles.info.size);
    }

    const AllocatedBuffer staging = resource_manager_->create_buffer(
        vertex_position_buffer_size + vertex_data_buffer_size + index_buffer_size
            + meshlet_buffer_size + meshlet_vertices_size + meshlet_triangles_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data;
    VK_CHECK(vmaMapMemory(gpu_->getAllocator(), staging.allocation, &data));

    // copy vertex buffer
    memcpy(data, vertex_positions.data(), vertex_position_buffer_size);
    memcpy(static_cast<char*>(data) + vertex_position_buffer_size, vertex_data.data(),
           vertex_data_buffer_size);
    // copy index buffer
    memcpy(static_cast<char*>(data) + vertex_position_buffer_size + vertex_data_buffer_size,
           indices.data(), index_buffer_size);
    memcpy(static_cast<char*>(data) + vertex_position_buffer_size + vertex_data_buffer_size
               + index_buffer_size,
           meshlets.data(), meshlet_buffer_size);
    memcpy(static_cast<char*>(data) + vertex_position_buffer_size + vertex_data_buffer_size
               + index_buffer_size + meshlet_buffer_size,
           meshlet_vertices.data(), meshlet_vertices_size);
    memcpy(static_cast<char*>(data) + vertex_position_buffer_size + vertex_data_buffer_size
               + index_buffer_size + meshlet_buffer_size + meshlet_vertices_size,
           meshlet_triangles.data(), meshlet_triangles_size);

    vmaUnmapMemory(gpu_->getAllocator(), staging.allocation);

    gpu_->immediateSubmit([&](VkCommandBuffer cmd) {
      VkBufferCopy copy_regions[6] = {};

      copy_regions[0].dstOffset = 0;
      copy_regions[0].srcOffset = 0;
      copy_regions[0].size = vertex_position_buffer_size;

      copy_regions[1].dstOffset = 0;
      copy_regions[1].srcOffset = copy_regions[0].srcOffset + copy_regions[0].size;
      copy_regions[1].size = vertex_data_buffer_size;

      copy_regions[2].dstOffset = 0;
      copy_regions[2].srcOffset = copy_regions[1].srcOffset + copy_regions[1].size;
      copy_regions[2].size = index_buffer_size;

      copy_regions[3].dstOffset = 0;
      copy_regions[3].srcOffset = copy_regions[2].srcOffset + copy_regions[2].size;
      copy_regions[3].size = meshlet_buffer_size;

      copy_regions[4].dstOffset = 0;
      copy_regions[4].srcOffset = copy_regions[3].srcOffset + copy_regions[3].size;
      copy_regions[4].size = meshlet_vertices_size;

      copy_regions[5].dstOffset = 0;
      copy_regions[5].srcOffset = copy_regions[4].srcOffset + copy_regions[4].size;
      copy_regions[5].size = meshlet_triangles_size;

      vkCmdCopyBuffer(cmd, staging.buffer, mesh_buffers.vertex_position_buffer.buffer, 1,
                      &copy_regions[0]);
      vkCmdCopyBuffer(cmd, staging.buffer, mesh_buffers.vertex_data_buffer.buffer, 1,
                      &copy_regions[1]);
      vkCmdCopyBuffer(cmd, staging.buffer, mesh_buffers.index_buffer.buffer, 1, &copy_regions[2]);
      vkCmdCopyBuffer(cmd, staging.buffer, mesh_buffers.meshlet_buffer.buffer, 1, &copy_regions[3]);
      vkCmdCopyBuffer(cmd, staging.buffer, mesh_buffers.meshlet_vertices.buffer, 1,
                      &copy_regions[4]);
      vkCmdCopyBuffer(cmd, staging.buffer, mesh_buffers.meshlet_triangles.buffer, 1,
                      &copy_regions[5]);
    });
    resource_manager_->destroy_buffer(staging);
  }

  void MeshSystem::update() {
    if (repository_->meshes.size() != meshes_) {
      meshes_ = repository_->meshes.size();

      /* TODO use this
      update_mesh(get_indices(), .get_vertices());*/
      upload_mesh();
    }

    mesh_draws_.clear();

    constexpr auto root_transform = glm::mat4(1.0f);

    traverse_scene(0, root_transform);

    if (mesh_draws_.empty()) {
      return;
    }

    const size_t mesh_draw_buffer_size = mesh_draws_.size() * sizeof(MeshDraw);

    const auto& mesh_buffers = repository_->get_buffer<MeshBuffers>();
    const auto frame = current_frame_index;

    if (mesh_buffers.mesh_draw_buffer[frame].info.size < mesh_draw_buffer_size) {
      fmt::println("mesh_draw_buffer size needs to be increased by {}",
                   mesh_draw_buffer_size - mesh_buffers.mesh_draw_buffer[frame].info.size);
    }
    {
      void* mapped_data;
      const VmaAllocation allocation = mesh_buffers.mesh_draw_buffer[frame].allocation;
      VK_CHECK(vmaMapMemory(gpu_->getAllocator(), allocation, &mapped_data));
      const auto mesh_draws = static_cast<MeshDraw*>(mapped_data);
      std::memcpy(mesh_draws, mesh_draws_.data(), mesh_draws_.size() * sizeof(MeshDraw));
      vmaUnmapMemory(gpu_->getAllocator(), mesh_buffers.mesh_draw_buffer[frame].allocation);
    }

  }

  void MeshSystem::traverse_scene(const Entity entity, const glm::mat4& parent_transform) {
    const auto& node = repository_->scene_graph.get(entity).value().get();
    if (!node.visible) {
      return;
    }

    const auto& transform = repository_->transform_components.get(entity)->get();
    const glm::mat4 world_transform  // TODO check if matrix vector is needed
        = parent_transform * repository_->model_matrices.get(transform.matrix);

    const auto& mesh_component = repository_->mesh_components.get(entity);
    if (mesh_component.has_value()) {
      const auto& mesh = repository_->meshes.get(mesh_component->get().mesh);

      for (const auto surface : mesh.surfaces) {
        const auto& material = repository_->materials.get(surface.material);

        MeshDraw draw{
            .position = glm::vec3(world_transform[3]),
            .scale = 1.f,
            .orientation = glm::vec4(world_transform[0]),
            .min = glm::vec3(surface.local_bounds.min),
            .meshlet_offset = surface.meshlet_offset,
            .max = glm::vec3(surface.local_bounds.max),
            .meshlet_count = surface.meshlet_count,
            .vertex_count = surface.vertex_count,
            .index_count = surface.index_count,
            .first_index = surface.first_index,
            .vertex_offset = surface.vertex_offset,
            .materialIndex = static_cast<uint32>(surface.material),
        };

        mesh_draws_.push_back(draw);

        if (material.config.transparent) {
          // mesh_task_commands_.push_back(task);
        } else {
        }
      }
    }

    for (const auto& childEntity : node.children) {
      traverse_scene(childEntity, world_transform);
    }
  }

  void MeshSystem::cleanup() {
    const auto& mesh_buffers = repository_->get_buffer<MeshBuffers>();
    resource_manager_->destroy_buffer(mesh_buffers.vertex_position_buffer);
    resource_manager_->destroy_buffer(mesh_buffers.vertex_data_buffer);
    resource_manager_->destroy_buffer(mesh_buffers.index_buffer);
    resource_manager_->destroy_buffer(mesh_buffers.meshlet_buffer);
    resource_manager_->destroy_buffer(mesh_buffers.meshlet_vertices);
    resource_manager_->destroy_buffer(mesh_buffers.meshlet_triangles);
    for (int i = 0; i < getFramesInFlight(); i++) {
      resource_manager_->destroy_buffer(mesh_buffers.meshlet_task_commands_buffer[i]);
      resource_manager_->destroy_buffer(mesh_buffers.mesh_draw_buffer[i]);
      resource_manager_->destroy_buffer(mesh_buffers.draw_count_buffer[i]);
    }
    repository_->vertex_data.clear();
    repository_->vertex_positions.clear();
    repository_->indices.clear();

    if (mesh_buffers.descriptor_layout != VK_NULL_HANDLE) {
      vkDestroyDescriptorSetLayout(gpu_->getDevice(), mesh_buffers.descriptor_layout, nullptr);
    }
    repository_->deregister_buffer<MeshBuffers>();
  }
}  // namespace gestalt::application