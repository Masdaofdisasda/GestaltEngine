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

  //TODO get these from the resource_manager_
  //sun light
  resource_manager_->get_database().add_light(light_component::DirectionalLight(glm::vec3(1.f, 0.957f, 0.917f),
                                              20.f, glm::vec3(-0.216, 0.941, -0.257)));

  // point lights
  constexpr int gridWidth = 5;
  constexpr int gridHeight = 5;
  constexpr int gridDepth = 5;
  constexpr float spacing = 20.0f;  // Spacing between lights in the grid
  constexpr glm::vec3 gridStart(-gridWidth * spacing / 2.0f, 0.0f,
                                -gridHeight * spacing / 2.0f);  // Starting position of the grid

  for (int x = 0; x < gridWidth; x++) {
    for (int z = 0; z < gridHeight; z++) {
      for (int y = 0; y < gridDepth; y++) {
        // Calculate the position for each light based on its grid coordinates
        glm::vec3 position = gridStart + glm::vec3(x * spacing, y * spacing, z * spacing);

        // Generate a unique color for each light
        // This example simply cycles through red, green, and blue colors
        glm::vec3 color = glm::vec3(0.0f);
        color[(x + z) % 3] = 1.0f;  // Simple way to alternate colors

        // Add the light to the database
        resource_manager_->get_database().add_light(
            light_component::PointLight(color, 5.0f, position));
      }
    }
  }

  auto& light_data = resource_manager_->light_data;
  light_data.dir_light_buffer = resource_manager_->create_buffer(sizeof(DirectionalLight), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   VMA_MEMORY_USAGE_CPU_TO_GPU);
  light_data.point_light_buffer = resource_manager_->create_buffer(sizeof(PointLight) * max_point_lights_, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                        VMA_MEMORY_USAGE_CPU_TO_GPU);
}
void light_system::update() {
  auto& light_data = resource_manager_->light_data;
  const auto& directional_light
      = resource_manager_->get_database().get_lights(light_type::directional).at(0).get();
  DirectionalLight dir_light;
  dir_light.color = directional_light.color;
  dir_light.intensity = directional_light.intensity;
  dir_light.direction = directional_light.direction;
  dir_light.enabled = true;

  void* mapped_data;
  VmaAllocation allocation = light_data.dir_light_buffer.allocation;
  VK_CHECK(vmaMapMemory(gpu_.allocator, allocation, &mapped_data));
  memcpy(mapped_data, &dir_light, sizeof(DirectionalLight));
  vmaUnmapMemory(gpu_.allocator, light_data.dir_light_buffer.allocation);

  std::vector<PointLight> point_lights;
  for (const auto& light_source : resource_manager_->get_database().get_lights(light_type::point)) {
    if (point_lights.size() == max_point_lights_) {
              break;
    }

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
}
void light_system::cleanup() {
  resource_manager_->destroy_buffer(resource_manager_->light_data.dir_light_buffer);
  resource_manager_->destroy_buffer(resource_manager_->light_data.point_light_buffer);
}