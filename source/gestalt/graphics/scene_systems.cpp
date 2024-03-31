#include "scene_systems.h"

#include <glm/detail/_noise.hpp>

struct DirectionalLight {
        glm::vec3 color;
        float intensity;
        glm::vec3 direction;
        uint32_t viewProj;
  };

static_assert(sizeof(DirectionalLight) == 32);

  struct PointLight {
        glm::vec3 color;
        float intensity;
        glm::vec3 position;
        bool enabled;
    };
  static_assert(sizeof(PointLight) == 32);

void light_system::prepare() {
        auto& light_data = resource_manager_->light_data;

      light_data.light_set
      = resource_manager_->descriptorPool.allocate(gpu_.device, light_data.light_layout);

        const size_t max_directional_lights
            = repository_->max_lights(light_type::directional);
        const size_t max_point_lights = repository_->max_lights(light_type::point);
        const size_t max_lights = max_directional_lights + max_point_lights;

        light_data.dir_light_buffer = resource_manager_->create_buffer(
            sizeof(DirectionalLight) * max_directional_lights, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU);
        light_data.point_light_buffer = resource_manager_->create_buffer(
            sizeof(PointLight) * max_point_lights, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU);
        light_data.view_proj_matrices = resource_manager_->create_buffer(
            sizeof(glm::mat4) * max_lights, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU);

  writer.clear();
        std::vector<VkDescriptorBufferInfo> dirBufferInfos;
  for (int i = 0; i < repository_->max_lights(light_type::directional);
             ++i) {
          VkDescriptorBufferInfo bufferInfo = {};
          bufferInfo.buffer = resource_manager_->light_data.dir_light_buffer.buffer;
          bufferInfo.offset = 32 * i;
          bufferInfo.range = 32;
          dirBufferInfos.push_back(bufferInfo);
        }
        writer.write_buffer_array(15, dirBufferInfos, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);

        std::vector<VkDescriptorBufferInfo> pointBufferInfos;
        for (int i = 0; i < repository_->max_lights(light_type::point); ++i) {
          VkDescriptorBufferInfo bufferInfo = {};
          bufferInfo.buffer = resource_manager_->light_data.point_light_buffer.buffer;
          bufferInfo.offset = 32 * i;
          bufferInfo.range = 32;
          pointBufferInfos.push_back(bufferInfo);
        }
        writer.write_buffer_array(16, pointBufferInfos, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);

        std::vector<VkDescriptorBufferInfo> lightViewProjBufferInfos;
        for (int i = 0; i < repository_->max_lights(light_type::directional)
                                + repository_->max_lights(light_type::point);
             ++i) {
          VkDescriptorBufferInfo bufferInfo = {};
          bufferInfo.buffer = resource_manager_->light_data.view_proj_matrices.buffer;
          bufferInfo.offset = 64 * i;
          bufferInfo.range = 64;
          lightViewProjBufferInfos.push_back(bufferInfo);
        }
        writer.write_buffer_array(17, lightViewProjBufferInfos, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                  0);
        writer.update_set(gpu_.device, resource_manager_->light_data.light_set);

  }

glm::mat4 light_system::calculate_sun_view_proj(const glm::vec3 direction) const {

  auto& [min, max, is_dirty]
      = repository_->scene_graph.get(root_entity).value().get().bounds;

  glm::vec3 center = (min + max) * 0.5f;
  glm::vec3 size = max - min;
  float radius = glm::length(size) * 0.5f;
  glm::vec3 lightPosition = center + direction * radius;

  // Calculate the tight orthographic frustum
  float halfWidth = radius;          // Width of the frustum
  float halfHeight = radius;         // Height of the frustum
  float nearPlane = 0.1f;            // Near plane distance
  float farPlane = radius * 2.f;    // Far plane distance

  // Calculate the view matrix for the light
  glm::mat4 lightView = glm::lookAt(lightPosition, center, glm::vec3(0.0f, 1.0f, 0.0f));

  // Calculate the projection matrix for the light
  glm::mat4 lightProjection
      = glm::orthoRH_ZO(-halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane, farPlane);
  /*
  const glm::vec3 ndc_test = lightProjection * lightView * glm::vec4(center, 1.0f);
  assert(ndc_test.x  >= 0 && ndc_test.x <= 1 && ndc_test.y >= 0 && ndc_test.y <= 1 && ndc_test.z >= 0 && ndc_test.z <= 1);*/

  return lightProjection * lightView;
}

bool has_dirty_light(const std::unordered_map<entity, light_component>& lights) {
   return std::any_of(lights.begin(), lights.end(),
                           [](const std::pair<entity, light_component>& light) { return light.second.is_dirty; });
} 

void light_system::update_directional_lights(std::unordered_map<entity, light_component>& lights) {

  auto& light_data = resource_manager_->light_data;

  std::vector<DirectionalLight> dir_lights;
  for (auto& light_source : lights) {
    auto& [entity, light] = light_source;
    if (light.type != light_type::directional) {
      continue;
    }

    auto& rotation = repository_->transform_components.get(entity).value().get().rotation;
    glm::vec3 direction = glm::normalize(glm::vec3(0,0,-1.f) * rotation);

    auto& view_proj = repository_->light_view_projections.get(light.light_view_projections.at(0));
    view_proj = calculate_sun_view_proj(direction);

    DirectionalLight dir_light = {};
    dir_light.color = light.color;
    dir_light.intensity = light.intensity;
    dir_light.direction = direction;
    dir_light.viewProj = light.light_view_projections.at(0);
    dir_lights.push_back(dir_light);

    light.is_dirty = false;
  }

  void* mapped_data;
  VK_CHECK(vmaMapMemory(gpu_.allocator, light_data.dir_light_buffer.allocation, &mapped_data));
  memcpy(mapped_data, dir_lights.data(), sizeof(DirectionalLight) * dir_lights.size());
  vmaUnmapMemory(gpu_.allocator, light_data.dir_light_buffer.allocation);
  resource_manager_->config_.lighting.num_dir_lights = dir_lights.size();
}

void light_system::update_point_lights(std::unordered_map<entity, light_component>& lights) {

  auto& light_data = resource_manager_->light_data;

  std::vector<PointLight> point_lights;
  for ( auto& light_source : lights) {
    auto& [entity, light] = light_source;
    if (light.type != light_type::point) {
      continue;
    }
    auto& position = repository_->transform_components.get(entity).value().get().position;

    PointLight point_light = {};
    point_light.color = light.color;
    point_light.intensity = light.intensity;
    point_light.position = position;
    point_light.enabled = true;
    point_lights.push_back(point_light);

    light.is_dirty = false;
  }

  void* mapped_data;
  VK_CHECK(vmaMapMemory(gpu_.allocator, light_data.point_light_buffer.allocation, &mapped_data));
  memcpy(mapped_data, point_lights.data(), sizeof(PointLight) * point_lights.size());
  vmaUnmapMemory(gpu_.allocator, light_data.point_light_buffer.allocation);
  resource_manager_->config_.lighting.num_point_lights = point_lights.size();
}

void light_system::update() {

  auto& light_components = repository_->light_components.components();
  if (has_dirty_light(light_components)) {
    update_directional_lights(light_components);
    update_point_lights(light_components);
    
  }

  // changes in the scene graph can affect the light view-projection matrices
  const auto& light_data = resource_manager_->light_data;
  const auto& matrices = repository_->light_view_projections.data();
  void* mapped_data;
  VK_CHECK(vmaMapMemory(gpu_.allocator, light_data.view_proj_matrices.allocation, &mapped_data));
  memcpy(mapped_data, matrices.data(), sizeof(glm::mat4) * matrices.size());
  vmaUnmapMemory(gpu_.allocator, light_data.view_proj_matrices.allocation);
}

void light_system::cleanup() {
  resource_manager_->destroy_buffer(resource_manager_->light_data.dir_light_buffer);
  resource_manager_->destroy_buffer(resource_manager_->light_data.point_light_buffer);
  resource_manager_->destroy_buffer(resource_manager_->light_data.view_proj_matrices);
}

glm::mat4 transform_system::get_model_matrix(const transform_component& transform) {
  return translate(glm::mat4(1.0f), transform.position) * mat4_cast(transform.rotation)
         * scale(glm::mat4(1.0f), glm::vec3(transform.scale));
}

void transform_system::prepare() {
  
}

void transform_system::mark_parent_dirty(entity entity) {
  if (entity == invalid_entity) {
    return;
  }

  auto& node = repository_->scene_graph.get(entity).value().get();
  if (node.bounds.is_dirty) {
    return;
  }

  node.bounds.is_dirty = true;
  mark_parent_dirty(node.parent);
}

void transform_system::mark_children_dirty(entity entity) {
  if (entity == invalid_entity) {
    return;
  }

  auto& node = repository_->scene_graph.get(entity).value().get();
  if (node.bounds.is_dirty) {
    return;
  }

  node.bounds.is_dirty = true;
  for (const auto& child : node.children) {
       mark_children_dirty(child);
  }
}

void transform_system::mark_as_dirty(entity entity) {
  const auto& node = repository_->scene_graph.get(entity).value().get();
  node.bounds.is_dirty = true;
  for (const auto& child : node.children) {
       mark_children_dirty(child);
  }
  mark_parent_dirty(node.parent);

}

void transform_system::update_aabb(const entity entity, const glm::mat4& parent_transform) {
  auto& node = repository_->scene_graph.get(entity).value().get();
  if (!node.bounds.is_dirty) {
    return;
  }
  auto& transform = repository_->transform_components.get(entity).value().get();
  if (node.parent != invalid_entity) {
    const auto& parent_transform_component
        = repository_->transform_components.get(node.parent).value().get();
    transform.parent_position = parent_transform_component.position;
    transform.parent_rotation = parent_transform_component.rotation;
    transform.parent_scale = parent_transform_component.scale;
  } else {
    transform.parent_position = transform.position;
    transform.parent_rotation = transform.rotation;
    transform.parent_scale = transform.scale;
  }

  const auto& mesh_optional = repository_->mesh_components.get(entity);

  AABB aabb;
  auto& [min, max, is_dirty] = aabb;

  if (mesh_optional.has_value()) {
    const auto& mesh = repository_->meshes.get(mesh_optional->get().mesh);
    min.x = std::min(min.x, mesh.local_bounds.min.x);
    min.y = std::min(min.y, mesh.local_bounds.min.y);
    min.z = std::min(min.z, mesh.local_bounds.min.z);

    max.x = std::max(max.x, mesh.local_bounds.max.x);
    max.y = std::max(max.y, mesh.local_bounds.max.y);
    max.z = std::max(max.z, mesh.local_bounds.max.z);
  } else {
    // nodes without mesh components should still influence the bounds
      min = glm::vec3(-0.0001f);
      max = glm::vec3(0.0001f);
  }
  const glm::mat4 local_matrix = get_model_matrix(transform);
  const glm::mat4 model_matrix
      = parent_transform * local_matrix;
  // Decompose model_matrix into translation (T) and 3x3 rotation matrix (M)
  glm::vec3 T = glm::vec3(model_matrix[3]);  // Translation vector
  glm::mat3 M = glm::mat3(model_matrix);     // Rotation matrix
  M = transpose(M);                     // Otherwise the AABB will be rotated in the wrong direction


  AABB transformedAABB = {T, T};  // Start with a zero-volume AABB at T

  // Applying Arvo's method to adjust AABB based on rotation and translation
  // NOTE: does not work for non-uniform scaling
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      float a = M[i][j] * min[j];
      float b = M[i][j] * max[j];
      transformedAABB.min[i] += a < b ? a : b;
      transformedAABB.max[i] += a < b ? b : a;
    }
  }
  min = transformedAABB.min;
  max = transformedAABB.max;

  for (const auto& child : node.children) {
    update_aabb(child, model_matrix);
    const auto& child_aabb = repository_->scene_graph.get(child).value().get().bounds;
    min.x = std::min(min.x, child_aabb.min.x);
    min.y = std::min(min.y, child_aabb.min.y);
    min.z = std::min(min.z, child_aabb.min.z);

    max.x = std::max(max.x, child_aabb.max.x);
    max.y = std::max(max.y, child_aabb.max.y);
    max.z = std::max(max.z, child_aabb.max.z);
  }

  node.bounds = aabb;
  node.bounds.is_dirty = false;
}

void transform_system::update() {

  for (auto& [entity, transform] : repository_->transform_components.components()) {
    if (transform.is_dirty) {
      mark_as_dirty(entity);
      repository_->model_matrices.set(transform.matrix, get_model_matrix(transform));
      transform.is_dirty = false;
    }
  }

   constexpr auto root_transform = glm::mat4(1.0f);

  update_aabb(root_entity, root_transform);

  // mark the directional light as dirty to update the view-projection matrix
  repository_->light_components.asVector().at(0).second.get().is_dirty = true;
}

 void transform_system::cleanup() {
  
}

void render_system::traverse_scene(const entity entity, const glm::mat4& parent_transform) {
   assert(entity != invalid_entity);
   const auto& node = repository_->scene_graph.get(entity).value().get();
   if (!node.visible) {
     return;
   }

   const auto& transform = repository_->transform_components.get(entity)->get();
   const glm::mat4 world_transform //TODO check if matrix vector is needed
       = parent_transform * repository_->model_matrices.get(transform.matrix);

   const auto& mesh_component = repository_->mesh_components.get(entity);
   if (mesh_component.has_value()) {
     const auto& mesh = repository_->meshes.get(mesh_component->get().mesh);

     for (const auto surface : mesh.surfaces) {
       const auto& material = repository_->materials.get(surface.material);

       render_object def;
       def.index_count = surface.index_count;
       def.first_index = surface.first_index;
       def.vertex_offset = surface.vertex_offset;
       def.material = surface.material;
       def.transform = world_transform;

       if (material.config.transparent) {
         resource_manager_->main_draw_context_.transparent_surfaces.push_back(def);
       } else {
         resource_manager_->main_draw_context_.opaque_surfaces.push_back(def);
       }
     }
   }

   for (const auto& childEntity : node.children) {
     traverse_scene(childEntity, world_transform);
   }
 }

void render_system::prepare() {
   const size_t initial_vertex_position_buffer_size = 184521 * sizeof(GpuVertexPosition);
   const size_t initial_vertex_data_buffer_size = 184521 * sizeof(GpuVertexData);
   const size_t initial_index_buffer_size = 786801 * sizeof(uint32_t);

   // Create initially empty vertex buffer
   resource_manager_->scene_geometry_.vertexPositionBuffer = resource_manager_->create_buffer(
       initial_vertex_position_buffer_size,
       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
            | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
       VMA_MEMORY_USAGE_GPU_ONLY);
   resource_manager_->scene_geometry_.vertexDataBuffer = resource_manager_->create_buffer(
       initial_vertex_data_buffer_size,
       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
           | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
       VMA_MEMORY_USAGE_GPU_ONLY);

   // Create initially empty index buffer
   resource_manager_->scene_geometry_.indexBuffer = resource_manager_->create_buffer(
       initial_index_buffer_size,
       VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
       VMA_MEMORY_USAGE_GPU_ONLY);
 }

void render_system::update() {
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

void hierarchy_system::prepare() {
}

void hierarchy_system::update() {
}

void hierarchy_system::cleanup() {
}

