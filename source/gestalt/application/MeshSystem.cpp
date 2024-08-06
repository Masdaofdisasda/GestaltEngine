#include "RenderConfig.hpp"
#include "SceneSystem.hpp"

#include <glm/gtx/matrix_decompose.hpp>

#include "descriptor.hpp"

namespace gestalt::application {

    constexpr size_t kMaxVertexPositionBufferSize
        = getMaxVertices() * sizeof(GpuVertexPosition);
    constexpr size_t kMaxVertexDataBufferSize = getMaxVertices() * sizeof(GpuVertexData);
    constexpr size_t kMaxIndexBufferSize = getMaxIndices() * sizeof(uint32);

  constexpr size_t kMaxMeshletBufferSize = getMaxMeshlets() * sizeof(Meshlet);
  constexpr size_t kMaxMeshletVertexBufferSize = getMaxVertices() * sizeof(uint32);
  constexpr size_t kMaxMeshletIndexBufferSize = getMaxIndices() * sizeof(uint8);

  constexpr size_t kMaxMeshletTaskCommandsBufferSize = getMaxMeshlets() * sizeof(MeshTaskCommand);
  constexpr size_t kMaxMeshDrawBufferSize = getMaxMeshes() * sizeof(MeshDraw);
  constexpr size_t kMaxDrawCountBufferSize = 4 * sizeof(uint32); // commandCount, x, y, z

  void MeshSystem::prepare() {

      create_buffers();
    fill_descriptors();

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

    const auto& mesh_buffers = repository_->mesh_buffers;

    if (kMaxVertexPositionBufferSize < vertex_position_buffer_size) {
      fmt::println("vertex_position_buffer size needs to be increased by {}",
                   vertex_position_buffer_size - mesh_buffers->vertex_position_buffer->info.size);
    }

    if (kMaxVertexDataBufferSize < vertex_data_buffer_size) {
      fmt::println("vertex_data_buffer size needs to be increased by {}",
                   vertex_data_buffer_size - mesh_buffers->vertex_data_buffer->info.size);
    }

    if (kMaxIndexBufferSize < index_buffer_size) {
      fmt::println("index_buffer size needs to be increased by {}",
                   index_buffer_size - mesh_buffers->index_buffer->info.size);
    }

    if (kMaxMeshletBufferSize < meshlet_buffer_size) {
      fmt::println("meshlet_buffer size needs to be increased by {}",
                   meshlet_buffer_size - mesh_buffers->meshlet_buffer->info.size);
    }

    if (kMaxMeshletVertexBufferSize < meshlet_vertices_size) {
      fmt::println("meshlet_vertices size needs to be increased by {}",
                   meshlet_buffer_size - mesh_buffers->meshlet_vertices->info.size);
    }

    if (kMaxMeshletIndexBufferSize < meshlet_triangles_size) {
      fmt::println("meshlet_triangles size needs to be increased by {}",
                   meshlet_triangles_size - mesh_buffers->meshlet_triangles->info.size);
    }

    const auto staging = resource_manager_->create_buffer(
        vertex_position_buffer_size + vertex_data_buffer_size + index_buffer_size
            + meshlet_buffer_size + meshlet_vertices_size + meshlet_triangles_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data;
    VK_CHECK(vmaMapMemory(gpu_->getAllocator(), staging->allocation, &data));

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

    vmaUnmapMemory(gpu_->getAllocator(), staging->allocation);

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

      vkCmdCopyBuffer(cmd, staging->buffer, mesh_buffers->vertex_position_buffer->buffer, 1,
                      &copy_regions[0]);
      vkCmdCopyBuffer(cmd, staging->buffer, mesh_buffers->vertex_data_buffer->buffer, 1,
                      &copy_regions[1]);
      vkCmdCopyBuffer(cmd, staging->buffer, mesh_buffers->index_buffer->buffer, 1,
                      &copy_regions[2]);
      vkCmdCopyBuffer(cmd, staging->buffer, mesh_buffers->meshlet_buffer->buffer, 1,
                      &copy_regions[3]);
      vkCmdCopyBuffer(cmd, staging->buffer, mesh_buffers->meshlet_vertices->buffer, 1,
                      &copy_regions[4]);
      vkCmdCopyBuffer(cmd, staging->buffer, mesh_buffers->meshlet_triangles->buffer, 1,
                      &copy_regions[5]);
    });
    resource_manager_->destroy_buffer(staging);
  }

  void MeshSystem::create_buffers() {
    const auto& mesh_buffers = repository_->mesh_buffers;

    descriptor_layout_builder_->clear();
    constexpr auto geometry_stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT
                                     | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_COMPUTE_BIT;
    const auto descriptor_layout
        = descriptor_layout_builder_
              ->add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, geometry_stages)
              .add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, geometry_stages)
              .add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, geometry_stages)
              .add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, geometry_stages)
              .add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, geometry_stages)
              .add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, geometry_stages)
              .add_binding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, geometry_stages)
              .add_binding(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, geometry_stages)
              .build(gpu_->getDevice(), VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

        for (int i = 0; i < getFramesInFlight(); i++) {
      mesh_buffers->descriptor_buffers[i] = resource_manager_->create_descriptor_buffer(
          descriptor_layout, 8, 0);
    }

    vkDestroyDescriptorSetLayout(gpu_->getDevice(), descriptor_layout, nullptr);

    // Create vertex buffer
    mesh_buffers->vertex_position_buffer = resource_manager_->create_buffer(
        kMaxVertexPositionBufferSize,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    mesh_buffers->vertex_data_buffer = resource_manager_->create_buffer(
        kMaxVertexDataBufferSize,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    // Create index buffer
    mesh_buffers->index_buffer = resource_manager_->create_buffer(
        kMaxIndexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    // Create meshlet buffers
    mesh_buffers->meshlet_buffer = resource_manager_->create_buffer(
        kMaxMeshletBufferSize,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER | VK_BUFFER_USAGE_TRANSFER_DST_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    mesh_buffers->meshlet_vertices = resource_manager_->create_buffer(
        kMaxMeshletVertexBufferSize,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER | VK_BUFFER_USAGE_TRANSFER_DST_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);
    mesh_buffers->meshlet_triangles = resource_manager_->create_buffer(
        kMaxMeshletIndexBufferSize,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER | VK_BUFFER_USAGE_TRANSFER_DST_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    // Create mesh task commands and draw buffers
    for (int i = 0; i < getFramesInFlight(); i++) {
      mesh_buffers->meshlet_task_commands_buffer[i] = resource_manager_->create_buffer(
          kMaxMeshletTaskCommandsBufferSize,
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
              | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
          VMA_MEMORY_USAGE_GPU_ONLY);
      mesh_buffers->mesh_draw_buffer[i] = resource_manager_->create_buffer(
          kMaxMeshDrawBufferSize,
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
          VMA_MEMORY_USAGE_CPU_TO_GPU);
      mesh_buffers->draw_count_buffer[i] = resource_manager_->create_buffer(
          kMaxDrawCountBufferSize,
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
              | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
          VMA_MEMORY_USAGE_GPU_ONLY);
    }
  }

  void MeshSystem::fill_descriptors() {
    const auto& mesh_buffers = repository_->mesh_buffers;

    for (int i = 0; i < getFramesInFlight(); i++) {
      mesh_buffers->descriptor_buffers[i]
          ->write_buffer(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                         mesh_buffers->vertex_position_buffer->address,
                         sizeof(GpuVertexPosition) * getMaxVertices())
          .write_buffer(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        mesh_buffers->vertex_data_buffer->address,
                        sizeof(GpuVertexData) * getMaxVertices())
          .write_buffer(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_buffers->meshlet_buffer->address,
                        sizeof(Meshlet) * getMaxMeshlets())
          .write_buffer(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        mesh_buffers->meshlet_vertices->address, sizeof(uint32) * getMaxVertices())
          .write_buffer(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        mesh_buffers->meshlet_triangles->address, sizeof(uint8) * getMaxIndices())
          .write_buffer(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        mesh_buffers->meshlet_task_commands_buffer[i]->address,
                        sizeof(MeshTaskCommand) * getMaxMeshlets())
          .write_buffer(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        mesh_buffers->mesh_draw_buffer[i]->address,
                        sizeof(MeshDraw) * getMaxMeshes())
          .write_buffer(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        mesh_buffers->draw_count_buffer[i]->address, 4 * sizeof(uint32))
          .update();
    }
  }

  void MeshSystem::update() {
    if (repository_->meshes.size() != meshes_) {
      meshes_ = repository_->meshes.size();

      /* TODO use this
      update_mesh(get_indices(), .get_vertices());*/
      upload_mesh();
    }

    constexpr auto root_transform = glm::mat4(1.0f);

    traverse_scene(0, root_transform);

    auto mesh_draws = repository_->mesh_draws;
    if (mesh_draws.size() == 0) {
      return;
    }

    const size_t mesh_draw_buffer_size = mesh_draws.size() * sizeof(MeshDraw);

    const auto& mesh_buffers = repository_->mesh_buffers;
    const auto frame = current_frame_index;

    if (kMaxMeshDrawBufferSize < mesh_draw_buffer_size) {
      fmt::println("mesh_draw_buffer size needs to be increased by {}",
                   mesh_draw_buffer_size - mesh_buffers->mesh_draw_buffer[frame]->info.size);
    }
    {
      void* mapped_data;
      const VmaAllocation allocation = mesh_buffers->mesh_draw_buffer[frame]->allocation;
      VK_CHECK(vmaMapMemory(gpu_->getAllocator(), allocation, &mapped_data));
      const auto mesh_draw_data = static_cast<MeshDraw*>(mapped_data);
      std::memcpy(mesh_draw_data, mesh_draws.data().data(), mesh_draws.size() * sizeof(MeshDraw));
      vmaUnmapMemory(gpu_->getAllocator(), mesh_buffers->mesh_draw_buffer[frame]->allocation);
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

        //TODO Replace this
        glm::vec3 position;
        glm::quat orientation;
        glm::vec3 scale;
        glm::vec3 skew;
        glm::vec4 perspective;
        decompose(world_transform, scale, orientation, position, skew, perspective);

        MeshDraw draw{
            .position = position,
            .scale = scale.x,
            .orientation = orientation,
            .center = glm::vec3(surface.local_bounds.center),
            .radius = surface.local_bounds.radius,
            .meshlet_offset = surface.meshlet_offset,
            .meshlet_count = surface.meshlet_count,
            .vertex_count = surface.vertex_count,
            .index_count = surface.index_count,
            .first_index = surface.first_index,
            .vertex_offset = surface.vertex_offset,
            .materialIndex = static_cast<uint32>(surface.material),
        };

        repository_->mesh_draws.get(surface.mesh_draw) = draw;

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
    const auto& mesh_buffers = repository_->mesh_buffers;
    resource_manager_->destroy_buffer(mesh_buffers->vertex_position_buffer);
    resource_manager_->destroy_buffer(mesh_buffers->vertex_data_buffer);
    resource_manager_->destroy_buffer(mesh_buffers->index_buffer);
    resource_manager_->destroy_buffer(mesh_buffers->meshlet_buffer);
    resource_manager_->destroy_buffer(mesh_buffers->meshlet_vertices);
    resource_manager_->destroy_buffer(mesh_buffers->meshlet_triangles);
    for (int i = 0; i < getFramesInFlight(); i++) {
      resource_manager_->destroy_buffer(mesh_buffers->meshlet_task_commands_buffer[i]);
      resource_manager_->destroy_buffer(mesh_buffers->mesh_draw_buffer[i]);
      resource_manager_->destroy_buffer(mesh_buffers->draw_count_buffer[i]);
    }
    repository_->vertex_data.clear();
    repository_->vertex_positions.clear();
    repository_->indices.clear();
  }
}  // namespace gestalt::application