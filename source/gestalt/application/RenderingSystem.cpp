#include "SceneSystem.hpp"

namespace gestalt::application {

  void RenderSystem::traverse_scene(const entity entity, const glm::mat4& parent_transform) {
    assert(entity != invalid_entity);
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

        RenderObject def;
        def.index_count = surface.index_count;
        def.first_index = surface.first_index;
        def.vertex_offset = surface.vertex_offset;
        def.material = surface.material;
        def.transform = world_transform;

        if (material.config.transparent) {
          repository_->main_draw_context_.transparent_surfaces.push_back(def);
        } else {
          repository_->main_draw_context_.opaque_surfaces.push_back(def);
        }
      }
    }

    for (const auto& childEntity : node.children) {
      traverse_scene(childEntity, world_transform);
    }
  }

  void RenderSystem::prepare() {
    constexpr size_t initial_vertex_position_buffer_size
        = getMaxVertices()
          * sizeof(GpuVertexPosition);
    constexpr size_t initial_vertex_data_buffer_size = getMaxVertices() * sizeof(GpuVertexData);
    constexpr size_t initial_index_buffer_size = getMaxIndices() * sizeof(uint32_t);

    MeshBuffers mesh_buffers;

    descriptor_layout_builder_->clear();
    mesh_buffers.descriptor_layout
        = descriptor_layout_builder_
              ->add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
              .add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
              .build(gpu_->getDevice());

    mesh_buffers.descriptor_set = resource_manager_->get_descriptor_pool()->allocate(
        gpu_->getDevice(), mesh_buffers.descriptor_layout);

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

    repository_->register_buffer(mesh_buffers);
  }

  void RenderSystem::upload_mesh() {
    const std::span indices = repository_->indices.data();
    const std::span vertex_positions = repository_->vertex_positions.data();
    const std::span vertex_data = repository_->vertex_data.data();

    const size_t vertex_position_buffer_size = vertex_positions.size() * sizeof(GpuVertexPosition);
    const size_t vertex_data_buffer_size = vertex_data.size() * sizeof(GpuVertexData);
    const size_t index_buffer_size = indices.size() * sizeof(uint32_t);

    auto& mesh_buffers = repository_->get_buffer<MeshBuffers>();

    if (mesh_buffers.vertex_position_buffer.info.size < vertex_position_buffer_size) {
      resource_manager_->destroy_buffer(mesh_buffers.vertex_position_buffer);
      mesh_buffers.vertex_position_buffer = resource_manager_->create_buffer(
          vertex_position_buffer_size,
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          VMA_MEMORY_USAGE_GPU_ONLY);
    }

    if (mesh_buffers.vertex_data_buffer.info.size < vertex_data_buffer_size) {
      resource_manager_->destroy_buffer(mesh_buffers.vertex_data_buffer);
      mesh_buffers.vertex_data_buffer = resource_manager_->create_buffer(
          vertex_data_buffer_size,
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                          VMA_MEMORY_USAGE_GPU_ONLY);
    }

    if (mesh_buffers.index_buffer.info.size < index_buffer_size) {
      resource_manager_->destroy_buffer(mesh_buffers.index_buffer);
      mesh_buffers.index_buffer = resource_manager_->create_buffer(
          index_buffer_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
          VMA_MEMORY_USAGE_GPU_ONLY);
    }

    const AllocatedBuffer staging
        = resource_manager_->create_buffer(vertex_position_buffer_size + vertex_data_buffer_size + index_buffer_size,
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

    vmaUnmapMemory(gpu_->getAllocator(), staging.allocation);

    gpu_->immediateSubmit([&](VkCommandBuffer cmd) {
      VkBufferCopy copy_regions[3] = {};

      copy_regions[0].dstOffset = 0;
      copy_regions[0].srcOffset = 0;
      copy_regions[0].size = vertex_position_buffer_size;

      copy_regions[1].dstOffset = 0;
      copy_regions[1].srcOffset = vertex_position_buffer_size;
      copy_regions[1].size = vertex_data_buffer_size;

      copy_regions[2].dstOffset = 0;
      copy_regions[2].srcOffset = vertex_position_buffer_size + vertex_data_buffer_size;
      copy_regions[2].size = index_buffer_size;

      vkCmdCopyBuffer(cmd, staging.buffer, mesh_buffers.vertex_position_buffer.buffer, 1,
                      &copy_regions[0]);
      vkCmdCopyBuffer(cmd, staging.buffer, mesh_buffers.vertex_data_buffer.buffer, 1,
                      &copy_regions[1]);
      vkCmdCopyBuffer(cmd, staging.buffer, mesh_buffers.index_buffer.buffer, 1, &copy_regions[2]);
    });
    resource_manager_->destroy_buffer(staging);

    writer_->clear();
    writer_->write_buffer(0, mesh_buffers.vertex_position_buffer.buffer,
                          vertex_position_buffer_size,
                        0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    writer_->write_buffer(1, mesh_buffers.vertex_data_buffer.buffer, vertex_data_buffer_size, 0,
                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    writer_->update_set(gpu_->getDevice(), mesh_buffers.descriptor_set);
  }

  void RenderSystem::update() {
    if (repository_->meshes.size() != meshes_) {
      meshes_ = repository_->meshes.size();

      /* TODO use this
      update_mesh(get_indices(), .get_vertices());*/
      upload_mesh();
    }

    constexpr auto root_transform = glm::mat4(1.0f);

    traverse_scene(0, root_transform);
  }

  void RenderSystem::cleanup() {
    const auto& mesh_buffers = repository_->get_buffer<MeshBuffers>();
    resource_manager_->destroy_buffer(mesh_buffers.vertex_position_buffer);
    resource_manager_->destroy_buffer(mesh_buffers.vertex_data_buffer);
    resource_manager_->destroy_buffer(mesh_buffers.index_buffer);
    repository_->vertex_data.clear();
     repository_->vertex_positions.clear();
     repository_->indices.clear();

    if (mesh_buffers.descriptor_layout != VK_NULL_HANDLE) {
      vkDestroyDescriptorSetLayout(gpu_->getDevice(), mesh_buffers.descriptor_layout, nullptr);
    }
    repository_->deregister_buffer<MeshBuffers>();

  }
}  // namespace gestalt::application