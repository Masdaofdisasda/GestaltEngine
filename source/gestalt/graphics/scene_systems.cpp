#include "scene_systems.h"

   struct DirectionalLight {
        glm::vec3 color;
        float intensity;
        glm::vec3 direction;
        bool enabled;
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
        light_data.dir_light_buffer = resource_manager_->create_buffer(
            sizeof(DirectionalLight)
                * resource_manager_->get_database().max_lights(light_type::directional),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                   VMA_MEMORY_USAGE_CPU_TO_GPU);
  light_data.point_light_buffer = resource_manager_->create_buffer(
      sizeof(PointLight) * resource_manager_->get_database().max_lights(light_type::point),
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                        VMA_MEMORY_USAGE_CPU_TO_GPU);
}

void light_system::update() {
  auto& light_data = resource_manager_->light_data;

  std::vector<DirectionalLight> dir_lights;
  for (const auto& light_source : resource_manager_->get_database().get_lights(light_type::directional)) {
       const auto& light = light_source.get();

    DirectionalLight dir_light = {};
    dir_light.color = light.color;
    dir_light.intensity = light.intensity;
    dir_light.direction = light.direction;
    dir_light.enabled = true;
    dir_lights.push_back(dir_light);
  }

  void* mapped_data;
  VmaAllocation allocation = light_data.dir_light_buffer.allocation;
  VK_CHECK(vmaMapMemory(gpu_.allocator, allocation, &mapped_data));
  memcpy(mapped_data, dir_lights.data(), sizeof(DirectionalLight) * dir_lights.size());
  vmaUnmapMemory(gpu_.allocator, light_data.dir_light_buffer.allocation);
  resource_manager_->config_.lighting.num_dir_lights = dir_lights.size();

  std::vector<PointLight> point_lights;
  for (const auto& light_source : resource_manager_->get_database().get_lights(light_type::point)) {
    const auto& light = light_source.get();

    PointLight point_light = {};
    point_light.color = light.color;
    point_light.intensity = light.intensity;
    point_light.position = light.position;
    point_light.enabled = true;
    point_lights.push_back(point_light);
  }

  VK_CHECK(vmaMapMemory(gpu_.allocator, light_data.point_light_buffer.allocation, &mapped_data));
  memcpy(mapped_data, point_lights.data(), sizeof(PointLight) * point_lights.size());
  vmaUnmapMemory(gpu_.allocator, light_data.point_light_buffer.allocation);
  resource_manager_->config_.lighting.num_point_lights = point_lights.size();
}
void light_system::cleanup() {
  resource_manager_->destroy_buffer(resource_manager_->light_data.dir_light_buffer);
  resource_manager_->destroy_buffer(resource_manager_->light_data.point_light_buffer);
}