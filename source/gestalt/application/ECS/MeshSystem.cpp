#include "MeshSystem.hpp"

#include <numeric>
#include <ranges>
#include <span>

#include "VulkanCheck.hpp"

#include "FrameProvider.hpp"
#include "Interface/IGpu.hpp"
#include "Interface/IResourceAllocator.hpp"
#include "Mesh/MeshSurface.hpp"
#include "Mesh/MeshTaskCommand.hpp"

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
                   vertex_position_buffer_size - mesh_buffers->vertex_position_buffer->get_size());
    }

    if (kMaxVertexDataBufferSize < vertex_data_buffer_size) {
      fmt::println("vertex_data_buffer size needs to be increased by {}",
                   vertex_data_buffer_size - mesh_buffers->vertex_data_buffer->get_size());
    }

    if (kMaxIndexBufferSize < index_buffer_size) {
      fmt::println("index_buffer size needs to be increased by {}",
                   index_buffer_size - mesh_buffers->index_buffer->get_size());
    }

    if (kMaxMeshletBufferSize < meshlet_buffer_size) {
      fmt::println("meshlet_buffer size needs to be increased by {}",
                   meshlet_buffer_size - mesh_buffers->meshlet_buffer->get_size());
    }

    if (kMaxMeshletVertexBufferSize < meshlet_vertices_size) {
      fmt::println("meshlet_vertices size needs to be increased by {}",
                   meshlet_buffer_size - mesh_buffers->meshlet_vertices->get_size());
    }

    if (kMaxMeshletIndexBufferSize < meshlet_triangles_size) {
      fmt::println("meshlet_triangles size needs to be increased by {}",
                   meshlet_triangles_size - mesh_buffers->meshlet_triangles->get_size());
    }

    const auto staging = resource_allocator_->create_buffer(std::move(
        BufferTemplate("Mesh Staging",
                       vertex_position_buffer_size + vertex_data_buffer_size + index_buffer_size
                           + meshlet_buffer_size + meshlet_vertices_size + meshlet_triangles_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)));

    void* data;
    VK_CHECK(vmaMapMemory(gpu_->getAllocator(), staging->get_allocation(), &data));

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

    vmaUnmapMemory(gpu_->getAllocator(), staging->get_allocation());

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

      vkCmdCopyBuffer(cmd, staging->get_buffer_handle(),
                      mesh_buffers->vertex_position_buffer->get_buffer_handle(), 1,
                      &copy_regions[0]);
      vkCmdCopyBuffer(cmd, staging->get_buffer_handle(),
                      mesh_buffers->vertex_data_buffer->get_buffer_handle(), 1,
                      &copy_regions[1]);
      vkCmdCopyBuffer(cmd, staging->get_buffer_handle(),
                      mesh_buffers->index_buffer->get_buffer_handle(), 1,
                      &copy_regions[2]);
      vkCmdCopyBuffer(cmd, staging->get_buffer_handle(),
                      mesh_buffers->meshlet_buffer->get_buffer_handle(), 1,
                      &copy_regions[3]);
      vkCmdCopyBuffer(cmd, staging->get_buffer_handle(),
                      mesh_buffers->meshlet_vertices->get_buffer_handle(), 1,
                      &copy_regions[4]);
      vkCmdCopyBuffer(cmd, staging->get_buffer_handle(),
                      mesh_buffers->meshlet_triangles->get_buffer_handle(), 1,
                      &copy_regions[5]);
    });
    resource_allocator_->destroy_buffer(staging);
  }

  void MeshSystem::create_buffers() {
    const auto& mesh_buffers = repository_->mesh_buffers;

    auto vertex_usage_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                              | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (isVulkanRayTracingEnabled()) {
      vertex_usage_flags |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    }
    mesh_buffers->vertex_position_buffer = resource_allocator_->create_buffer(
        BufferTemplate("Vertex Positions Storage Buffer", kMaxVertexPositionBufferSize, vertex_usage_flags,
                       0, VMA_MEMORY_USAGE_AUTO, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    mesh_buffers->vertex_data_buffer = resource_allocator_->create_buffer(
        BufferTemplate("Vertex Data Storage Buffer", kMaxVertexDataBufferSize,
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                           | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                       0, VMA_MEMORY_USAGE_AUTO, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    auto index_usage_flags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (isVulkanRayTracingEnabled()) {
      index_usage_flags |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    }
    mesh_buffers->index_buffer = resource_allocator_->create_buffer(
        BufferTemplate("Index Storage Buffer", kMaxIndexBufferSize, index_usage_flags, 0,
                       VMA_MEMORY_USAGE_AUTO, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    mesh_buffers->meshlet_buffer = resource_allocator_->create_buffer(
        BufferTemplate("Meshlet Storage Buffer", kMaxMeshletBufferSize,
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0,
                       VMA_MEMORY_USAGE_AUTO, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    mesh_buffers->meshlet_vertices = resource_allocator_->create_buffer(
        BufferTemplate("Meshlet Vertex Storage Buffer", kMaxMeshletVertexBufferSize,
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0,
                       VMA_MEMORY_USAGE_AUTO, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    mesh_buffers->meshlet_triangles = resource_allocator_->create_buffer(
        BufferTemplate("Meshlet Triangle Storage Buffer", kMaxMeshletIndexBufferSize,
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0,
                       VMA_MEMORY_USAGE_AUTO, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    mesh_buffers->meshlet_task_commands_buffer = resource_allocator_->create_buffer(
        BufferTemplate("Meshlet Task Commands Storage Buffer", kMaxMeshletTaskCommandsBufferSize,
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, 0,
                       VMA_MEMORY_USAGE_AUTO, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    mesh_buffers->mesh_draw_buffer = resource_allocator_->create_buffer(BufferTemplate(
        "Mesh Draws Storage Buffer", kMaxMeshDrawBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VMA_MEMORY_USAGE_AUTO,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
    mesh_buffers->command_count_buffer = resource_allocator_->create_buffer(
        BufferTemplate("Draw Command Count Storage Buffer", kMaxDrawCountBufferSize,
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0,
                       VMA_MEMORY_USAGE_AUTO, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
    mesh_buffers->group_count_buffer = resource_allocator_->create_buffer(
        BufferTemplate("Group Count Storage Buffer", kMaxDrawCountBufferSize,
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
                           | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                       0, VMA_MEMORY_USAGE_AUTO, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    mesh_buffers->luminance_histogram_buffer = resource_allocator_->create_buffer(
        BufferTemplate("Luminance Histogram Buffer", 256 * sizeof(uint32), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                       0, VMA_MEMORY_USAGE_GPU_ONLY, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
  }

  void MeshSystem::update(float delta_time, const UserInput& movement, float aspect) {
    if (repository_->meshes.size() != meshes_) {
      meshes_ = repository_->meshes.size();

      /* TODO use this
      update_mesh(get_indices(), .get_vertices());*/
      upload_mesh();
    }

    constexpr auto root_transform = TransformComponent{
        false,
        glm::vec3(0),
        glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        1.f,
        glm::vec3(0),
        glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        1.f,
    };

    repository_->mesh_draws_.clear();
    traverse_scene(0, root_transform);

    if (repository_->mesh_draws_.empty()) {
      return;
    }

    const size_t mesh_draw_buffer_size = repository_->mesh_draws_.size() * sizeof(MeshDraw);

    const auto& mesh_buffers = repository_->mesh_buffers;

    if (kMaxMeshDrawBufferSize < mesh_draw_buffer_size) {
      fmt::println("mesh_draw_buffer size needs to be increased by {}",
                   mesh_draw_buffer_size - mesh_buffers->mesh_draw_buffer->get_size());
    }

    mesh_buffers->mesh_draw_buffer->copy_to_mapped(
        gpu_->getAllocator(), repository_->mesh_draws_.data(),
                                                   mesh_draw_buffer_size);

  }

  void MeshSystem::traverse_scene(const Entity entity, const TransformComponent& parent_transform) {
    const auto& node = repository_->scene_graph.get(entity).value().get();
    if (!node.visible) {
      return;
    }

    const auto& local_transform = repository_->transform_components.get(entity)->get();

    TransformComponent world_transform = parent_transform * local_transform;

    const auto& mesh_component = repository_->mesh_components.get(entity);
    if (mesh_component.has_value()) {
      const auto& mesh = repository_->meshes.get(mesh_component->get().mesh);

      for (const auto surface : mesh.surfaces) {
        const auto& material = repository_->materials.get(surface.material);

        MeshDraw draw{
            .position = world_transform.position,
            .scale = world_transform.scale,
            .orientation = world_transform.rotation,
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

        repository_->mesh_draws_.push_back(draw);

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
    resource_allocator_->destroy_buffer(mesh_buffers->vertex_position_buffer);
    resource_allocator_->destroy_buffer(mesh_buffers->vertex_data_buffer);
    resource_allocator_->destroy_buffer(mesh_buffers->index_buffer);
    resource_allocator_->destroy_buffer(mesh_buffers->meshlet_buffer);
    resource_allocator_->destroy_buffer(mesh_buffers->meshlet_vertices);
    resource_allocator_->destroy_buffer(mesh_buffers->meshlet_triangles);

      resource_allocator_->destroy_buffer(mesh_buffers->meshlet_task_commands_buffer);
      resource_allocator_->destroy_buffer(mesh_buffers->mesh_draw_buffer);
      resource_allocator_->destroy_buffer(mesh_buffers->command_count_buffer);
      resource_allocator_->destroy_buffer(mesh_buffers->group_count_buffer);

    repository_->vertex_data.clear();
    repository_->vertex_positions.clear();
    repository_->indices.clear();
  }
}  // namespace gestalt::application