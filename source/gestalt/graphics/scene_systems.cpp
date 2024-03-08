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

void light_system::update(const std::vector<entity_component> entities) {

  std::vector<std::reference_wrapper<light_component>> directional_lights;
  std::vector<std::reference_wrapper<light_component>> points_lights;

  for (const auto& entity : entities) {
    if (entity.has_light()) {
      light_component& light = resource_manager_->get_database().get_light(entity.light);
      if (light.type == light_type::directional) {
        directional_lights.push_back(std::ref(light));
      } else if (light.type == light_type::point) {
        points_lights.push_back(std::ref(light));
      }
    }
  }

  update_directional_lights(directional_lights);
  update_point_lights(points_lights);

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