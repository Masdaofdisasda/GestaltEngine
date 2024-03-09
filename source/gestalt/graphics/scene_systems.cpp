#include "scene_systems.h"

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
        const size_t max_directional_lights
            = resource_manager_->get_database().max_lights(light_type::directional);
        const size_t max_point_lights
            = resource_manager_->get_database().max_lights(light_type::point);
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
  }

glm::mat4 light_system::calculate_sun_view_proj(const light_component& light) {

  // Define the dimensions of the orthographic projection
  auto config = resource_manager_->config_.shadow;
  glm::vec3 direction = normalize(light.direction);  // Ensure the direction is normalized

  glm::vec3 up = glm::vec3(0, 1, 0);
  if (glm::abs(dot(up, direction)) > 0.999f) {
    // up = glm::vec3(1, 0, 0);  // Switch to a different up vector if the initial choice is
    // parallel
  }

  // Create a view matrix for the light
  glm::mat4 lightView = lookAt(glm::vec3(0, 0, 0), -direction, glm::vec3(0, 0, 1));

  glm::vec3 min{config.min_corner};
  glm::vec3 max{config.max_corner};

  glm::vec3 corners[] = {
      glm::vec3(min.x, min.y, min.z), glm::vec3(min.x, max.y, min.z),
      glm::vec3(min.x, min.y, max.z), glm::vec3(min.x, max.y, max.z),
      glm::vec3(max.x, min.y, min.z), glm::vec3(max.x, max.y, min.z),
      glm::vec3(max.x, min.y, max.z), glm::vec3(max.x, max.y, max.z),
  };
  for (auto& v : corners) v = glm::vec3(lightView * glm::vec4(v, 1.0f));

  glm::vec3 vmin(std::numeric_limits<float>::max());
  glm::vec3 vmax(std::numeric_limits<float>::lowest());

  for (auto& corner : corners) {
    vmin = glm::min(vmin, corner);
    vmax = glm::max(vmax, corner);
  }
  glm::mat4 lightProjection = glm::orthoRH_ZO(min.x, max.x, min.y, max.y, -max.z, -min.z);

  lightProjection[1][1] *= -1;

  return lightProjection * lightView;
}

void light_system::update_directional_lights(
    const std::vector<std::reference_wrapper<light_component>>& lights) {
  auto& light_data = resource_manager_->light_data;

  std::vector<DirectionalLight> dir_lights;
  for (const auto& light_source : lights) {
    const auto& light = light_source.get();

    auto& view_proj = resource_manager_->get_database().get_light_view_proj(light.light_view_projections.at(0));
    view_proj = calculate_sun_view_proj(light);

    DirectionalLight dir_light = {};
    dir_light.color = light.color;
    dir_light.intensity = light.intensity;
    dir_light.direction = light.direction;
    dir_light.viewProj = light.light_view_projections.at(0);
    dir_lights.push_back(dir_light);
  }

  void* mapped_data;
  VK_CHECK(vmaMapMemory(gpu_.allocator, light_data.dir_light_buffer.allocation, &mapped_data));
  memcpy(mapped_data, dir_lights.data(), sizeof(DirectionalLight) * dir_lights.size());
  vmaUnmapMemory(gpu_.allocator, light_data.dir_light_buffer.allocation);
  resource_manager_->config_.lighting.num_dir_lights = dir_lights.size();
}

void light_system::update_point_lights(
    const std::vector<std::reference_wrapper<light_component>>& lights) {
  auto& light_data = resource_manager_->light_data;

  std::vector<PointLight> point_lights;
  for (const auto& light_source : lights) {
    const auto& light = light_source.get();

    PointLight point_light = {};
    point_light.color = light.color;
    point_light.intensity = light.intensity;
    point_light.position = light.position;
    point_light.enabled = true;
    point_lights.push_back(point_light);
  }

  void* mapped_data;
  VK_CHECK(vmaMapMemory(gpu_.allocator, light_data.point_light_buffer.allocation, &mapped_data));
  memcpy(mapped_data, point_lights.data(), sizeof(PointLight) * point_lights.size());
  vmaUnmapMemory(gpu_.allocator, light_data.point_light_buffer.allocation);
  resource_manager_->config_.lighting.num_point_lights = point_lights.size();
}

void light_system::update() {

  update_directional_lights(resource_manager_->get_database().get_lights(light_type::directional));
  update_point_lights(resource_manager_->get_database().get_lights(light_type::point));

  const auto& light_data = resource_manager_->light_data;
  const auto& matrices = resource_manager_->get_database().get_light_view_projs();
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

    return translate(transform.position) * mat4_cast(transform.rotation) * glm::scale(transform.scale);
}

void transform_system::prepare() {
  
}

void transform_system::update() {

  for (auto& transform : resource_manager_->get_database().get_transforms() | std::views::values) {
    if (transform.is_dirty) {
      resource_manager_->get_database().set_matrix(transform.matrix, get_model_matrix(transform));

      transform.is_dirty = false;
    }
  }
}
 void transform_system::cleanup() {
  
}

void render_system::traverse_scene(const entity entity, const glm::mat4& parent_transform) {
   assert(entity != invalid_entity);
   const auto& node = resource_manager_->get_database().get_node_component(entity).value().get();
   if (!node.visible) {
     return;
   }

   const auto& transform = resource_manager_->get_database().get_transform_component(entity)->get();
   const glm::mat4 world_transform
       = parent_transform * resource_manager_->get_database().get_matrix(transform.matrix);

   const auto& mesh_component = resource_manager_->get_database().get_mesh_component(entity);
   if (mesh_component.has_value()) {
     const auto& mesh = resource_manager_->get_database().get_mesh(mesh_component->get().mesh);

     for (const auto surface_index : mesh.surfaces) {
       const auto& surface = resource_manager_->get_database().get_surface(surface_index);

       const auto& material = resource_manager_->get_database().get_material(surface.material);

       render_object def;
       def.index_count = surface.index_count;
       def.first_index = surface.first_index;
       def.material = surface.material;
       def.bounds = surface.bounds;
       def.transform = world_transform;
       def.vertex_buffer_address = resource_manager_->scene_geometry_.vertexBufferAddress;

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
   const size_t initial_vertex_buffer_size = 184521 * sizeof(Vertex);
   const size_t initial_index_buffer_size = 786801 * sizeof(uint32_t);

   // Create initially empty vertex buffer
   resource_manager_->scene_geometry_.vertexBuffer = resource_manager_->create_buffer(
       initial_vertex_buffer_size,
       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
           | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
       VMA_MEMORY_USAGE_GPU_ONLY);

   // Create initially empty index buffer
   resource_manager_->scene_geometry_.indexBuffer = resource_manager_->create_buffer(
       initial_index_buffer_size,
       VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
       VMA_MEMORY_USAGE_GPU_ONLY);
 }

void render_system::update() {
   if (resource_manager_->get_database().get_meshes_size() != meshes_) {
     meshes_ = resource_manager_->get_database().get_meshes_size();

     /* TODO use this
     resource_manager_->update_mesh(resource_manager_->get_database().get_indices(),
                                    resource_manager_->get_database().get_vertices());*/
     resource_manager_->upload_mesh();
   }

   constexpr glm::mat4 identity = glm::mat4(1.0f);

   traverse_scene(0, identity);
 }
