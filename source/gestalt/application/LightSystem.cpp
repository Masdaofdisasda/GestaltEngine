#include "SceneSystem.hpp"

#include "VulkanCheck.hpp"
#include "Interface/IDescriptorLayoutBuilder.hpp"
#include "Interface/IGpu.hpp"
#include "Interface/IResourceManager.hpp"

namespace gestalt::application {

  void LightSystem::prepare() {

    notification_manager_->subscribe(
        ChangeType::ComponentUpdated, [this](const ChangeEvent& event) {
          if (repository_->light_components.get(event.entityId).has_value()) {
            updatable_entities_.push_back(event.entityId);
          }
        });

    create_buffers();
    fill_buffers();
  }

  void LightSystem::create_buffers() {
    const auto& light_data = repository_->light_buffers;

    descriptor_layout_builder_->clear();
    const auto descriptor_layout
        = descriptor_layout_builder_
              ->add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                            VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, false,
                            getMaxLights())
              .add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT,
                           false, getMaxDirectionalLights())
              .add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT,
                           false, getMaxPointLights())
              .build(gpu_->getDevice(), VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);


    light_data->descriptor_buffer = resource_manager_->create_descriptor_buffer(
        descriptor_layout, 3, 0);
    vkDestroyDescriptorSetLayout(gpu_->getDevice(), descriptor_layout, nullptr);

    light_data->view_proj_matrices = resource_manager_->create_buffer(
        sizeof(glm::mat4) * (getMaxDirectionalLights() + 6 * getMaxPointLights()),
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU, "Light Matrix Buffer");
    light_data->dir_light_buffer = resource_manager_->create_buffer(
        sizeof(GpuDirectionalLight) * getMaxDirectionalLights(),
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU, "Direction Lights Buffer");
    light_data->point_light_buffer = resource_manager_->create_buffer(
        sizeof(GpuPointLight) * getMaxPointLights(),
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU, "Point Light Buffer");

  }

  void LightSystem::fill_buffers() {
    const auto& light_data = repository_->light_buffers;

    light_data->descriptor_buffer
        ->write_buffer(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                       light_data->view_proj_matrices->address,
                       sizeof(glm::mat4) * getMaxDirectionalLights())
        .write_buffer(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, light_data->dir_light_buffer->address,
                      sizeof(GpuDirectionalLight) * getMaxDirectionalLights())
        .write_buffer(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, light_data->point_light_buffer->address,
                      sizeof(GpuPointLight) * getMaxPointLights())
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

  void LightSystem::update() {

    repository_->light_view_projections.clear();
    repository_->directional_lights.clear();
    repository_->point_lights.clear();

    for (auto [entity, Light_component] : repository_->light_components.asVector()) {
      if (Light_component.get().type == LightType::kDirectional) {
        auto& light = Light_component.get();
        const auto& rotation = repository_->transform_components[entity].rotation;
        auto& dir_light_data = std::get<DirectionalLightData>(Light_component.get().specific);
        glm::vec3 direction = normalize(glm::vec3(0, 0, -1.f) * rotation);

        auto view_proj = calculate_sun_view_proj(direction);
        repository_->light_view_projections.add(view_proj);
        dir_light_data.light_view_projection = repository_->light_view_projections.size() - 1;

        GpuDirectionalLight dir_light = {};
        dir_light.color = light.base.color;
        dir_light.intensity = light.base.intensity;
        dir_light.direction = direction;
        dir_light.viewProj = dir_light_data.light_view_projection;
        repository_->directional_lights.add(dir_light);

        light.is_dirty = false;
      } else if (Light_component.get().type == LightType::kPoint) {
        auto& light = Light_component.get();
        const auto& position = repository_->transform_components[entity].position;
        const auto& point_light_data = std::get<PointLightData>(light.specific);

        // TODO Calculate the 6 view matrices for the light

        GpuPointLight point_light = {};
        point_light.color = light.base.color;
        point_light.intensity = light.base.intensity;
        point_light.position = position;
        point_light.range = point_light_data.range;
        repository_->point_lights.add(point_light);

        light.is_dirty = false;
      }
    }
    
    auto& light_data = repository_->light_buffers;
    
    void* directional_light_data;
    VK_CHECK(vmaMapMemory(gpu_->getAllocator(), light_data->dir_light_buffer->allocation,
                          &directional_light_data));
    memcpy(directional_light_data, repository_->directional_lights.data().data(),
           sizeof(GpuDirectionalLight) * repository_->directional_lights.size());
    vmaUnmapMemory(gpu_->getAllocator(), light_data->dir_light_buffer->allocation);

    void* point_light_data;
    VK_CHECK(vmaMapMemory(gpu_->getAllocator(), light_data->point_light_buffer->allocation,
                          &point_light_data));
    memcpy(point_light_data, repository_->point_lights.data().data(),
           sizeof(GpuPointLight) * repository_->point_lights.size());
    vmaUnmapMemory(gpu_->getAllocator(), light_data->point_light_buffer->allocation);

    // changes in the scene graph can affect the light view-projection matrices
    void* view_matrix_data;
    VK_CHECK(vmaMapMemory(gpu_->getAllocator(), light_data->view_proj_matrices->allocation,
                          &view_matrix_data));
    memcpy(view_matrix_data, repository_->light_view_projections.data().data(),
           sizeof(glm::mat4) * repository_->light_view_projections.size());
    vmaUnmapMemory(gpu_->getAllocator(), light_data->view_proj_matrices->allocation);

    updatable_entities_.clear();
  }

  void LightSystem::cleanup() {
    repository_->directional_lights.clear();
    repository_->point_lights.clear();
    repository_->light_view_projections.clear();

    const auto& light_buffers = repository_->light_buffers;
    resource_manager_->destroy_descriptor_buffer(light_buffers->descriptor_buffer);
    resource_manager_->destroy_buffer(light_buffers->dir_light_buffer);
    resource_manager_->destroy_buffer(light_buffers->point_light_buffer);
    resource_manager_->destroy_buffer(light_buffers->view_proj_matrices);

  }

}  // namespace gestalt::application