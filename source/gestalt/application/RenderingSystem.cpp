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
    const size_t initial_vertex_position_buffer_size
        = 184521 * sizeof(GpuVertexPosition);  // Note these are the min values to load bistro scene
    const size_t initial_vertex_data_buffer_size = 184521 * sizeof(GpuVertexData);
    const size_t initial_index_buffer_size = 786801 * sizeof(uint32_t);

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

  void RenderSystem::update() {
    if (repository_->meshes.size() != meshes_) {
      meshes_ = repository_->meshes.size();

      /* TODO use this
      resource_manager_->update_mesh(resource_manager_->get_database().get_indices(),
                                     resource_manager_->get_database().get_vertices());*/
      resource_manager_->upload_mesh();
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