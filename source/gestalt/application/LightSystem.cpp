#include "SceneSystem.hpp"

#include <glm/detail/_noise.hpp>

#include "descriptor.hpp"

namespace gestalt::application {

  void LightSystem::prepare() {
    create_buffers();
    fill_buffers();
  }

  void LightSystem::create_buffers() {
    const auto& light_data = repository_->light_buffers;

    descriptor_layout_builder_->clear();
    const auto descriptor_layout
        = descriptor_layout_builder_
              ->add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT,
                            false, getMaxDirectionalLights())
              .add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT,
                           false, getMaxPointLights())
              .add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                           VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, false,
                           getMaxLights())
              .build(gpu_->getDevice(), VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);


    light_data->descriptor_buffer = resource_manager_->create_descriptor_buffer(
        descriptor_layout, 3, 0);
    vkDestroyDescriptorSetLayout(gpu_->getDevice(), descriptor_layout, nullptr);

    light_data->dir_light_buffer = resource_manager_->create_buffer(
        sizeof(GpuDirectionalLight) * getMaxDirectionalLights(),
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU);
    light_data->point_light_buffer = resource_manager_->create_buffer(
        sizeof(GpuPointLight) * getMaxPointLights(),
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU);
    light_data->view_proj_matrices = resource_manager_->create_buffer(
        sizeof(glm::mat4) * getMaxLights(),
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU);

  }

  void LightSystem::fill_buffers() {
    const auto& light_data = repository_->light_buffers;

    light_data->descriptor_buffer
        ->write_buffer_array(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                             light_data->dir_light_buffer->address,
                             sizeof(GpuDirectionalLight) * getMaxDirectionalLights())
        .write_buffer_array(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                            light_data->point_light_buffer->address,
                            sizeof(GpuPointLight) * getMaxPointLights())
        .write_buffer_array(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                            light_data->view_proj_matrices->address,
                            sizeof(glm::mat4) * getMaxLights())
        .update();
  }

  glm::mat4 LightSystem::calculate_sun_view_proj(const glm::vec3 direction) const {
    auto& [min, max, is_dirty] = repository_->scene_graph.get(root_entity).value().get().bounds;

    glm::vec3 center = (min + max) * 0.5f;
    glm::vec3 size = max - min;
    float radius = length(size) * 0.5f;
    glm::vec3 lightPosition = center + direction * radius;

    // Calculate the tight orthographic frustum
    float halfWidth = radius;       // Width of the frustum
    float halfHeight = radius;      // Height of the frustum
    float nearPlane = 0.1f;         // Near plane distance
    float farPlane = radius * 2.f;  // Far plane distance

    // Calculate the view matrix for the light
    glm::mat4 lightView = lookAt(lightPosition, center, glm::vec3(0.0f, 1.0f, 0.0f));

    // Calculate the projection matrix for the light
    glm::mat4 lightProjection
        = glm::orthoRH_ZO(-halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane, farPlane);
    /*
    const glm::vec3 ndc_test = lightProjection * lightView * glm::vec4(center, 1.0f);
    assert(ndc_test.x  >= 0 && ndc_test.x <= 1 && ndc_test.y >= 0 && ndc_test.y <= 1 && ndc_test.z
    >= 0 && ndc_test.z <= 1);*/

    return lightProjection * lightView;
  }

  bool has_dirty_light(const std::unordered_map<Entity, LightComponent>& lights) {
    return std::any_of(
        lights.begin(), lights.end(),
        [](const std::pair<Entity, LightComponent>& light) { return light.second.is_dirty; });
  }

  void LightSystem::update_directional_lights(std::unordered_map<Entity, LightComponent>& lights) {
    auto& light_data = repository_->light_buffers;

    repository_->directional_lights.clear();
    for (auto& light_source : lights) {
      auto& [entity, light] = light_source;
      if (light.type != LightType::kDirectional) {
        continue;
      }

      auto& rotation = repository_->transform_components.get(entity).value().get().rotation;
      glm::vec3 direction = normalize(glm::vec3(0, 0, -1.f) * rotation);

      auto& view_proj = repository_->light_view_projections.get(light.light_view_projections.at(0));
      view_proj = calculate_sun_view_proj(direction);

      GpuDirectionalLight dir_light = {};
      dir_light.color = light.color;
      dir_light.intensity = light.intensity;
      dir_light.direction = direction;
      dir_light.viewProj = light.light_view_projections.at(0);
      repository_->directional_lights.add(dir_light);

      light.is_dirty = false;
    }

    void* mapped_data;
    VK_CHECK(
        vmaMapMemory(gpu_->getAllocator(), light_data->dir_light_buffer->allocation, &mapped_data));
    memcpy(mapped_data, repository_->directional_lights.data().data(),
           sizeof(GpuDirectionalLight) * repository_->directional_lights.size());
    vmaUnmapMemory(gpu_->getAllocator(), light_data->dir_light_buffer->allocation);
  }

  void LightSystem::update_point_lights(std::unordered_map<Entity, LightComponent>& lights) {
    auto& light_data = repository_->light_buffers;

    repository_->point_lights.clear();
    for (auto& light_source : lights) {
      auto& [entity, light] = light_source;
      if (light.type != LightType::kPoint) {
        continue;
      }
      auto& position = repository_->transform_components.get(entity).value().get().position;

      GpuPointLight point_light = {};
      point_light.color = light.color;
      point_light.intensity = light.intensity;
      point_light.position = position;
      point_light.enabled = true;
      repository_->point_lights.add(point_light);

      light.is_dirty = false;
    }

    void* mapped_data;
    VK_CHECK(vmaMapMemory(gpu_->getAllocator(), light_data->point_light_buffer->allocation,
                          &mapped_data));
    memcpy(mapped_data, repository_->point_lights.data().data(),
           sizeof(GpuPointLight) * repository_->point_lights.size());
    vmaUnmapMemory(gpu_->getAllocator(), light_data->point_light_buffer->allocation);
  }

  void LightSystem::update() {
    auto& light_components = repository_->light_components.components();
    if (has_dirty_light(light_components)) {
      update_directional_lights(light_components);
      update_point_lights(light_components);
    }

    // changes in the scene graph can affect the light view-projection matrices
    const auto& light_data = repository_->light_buffers;
    const auto& matrices = repository_->light_view_projections.data();
    void* mapped_data;
    VK_CHECK(vmaMapMemory(gpu_->getAllocator(), light_data->view_proj_matrices->allocation,
                          &mapped_data));
    memcpy(mapped_data, matrices.data(), sizeof(glm::mat4) * matrices.size());
    vmaUnmapMemory(gpu_->getAllocator(), light_data->view_proj_matrices->allocation);
  }

  void LightSystem::cleanup() {
    repository_->directional_lights.clear();
    repository_->point_lights.clear();
    repository_->light_view_projections.clear();

    const auto& light_buffers = repository_->light_buffers;
    resource_manager_->destroy_buffer(light_buffers->dir_light_buffer);
    resource_manager_->destroy_buffer(light_buffers->point_light_buffer);
    resource_manager_->destroy_buffer(light_buffers->view_proj_matrices);
  }

}  // namespace gestalt::application