﻿#include "SceneSystem.hpp"

#include <glm/detail/_noise.hpp>

namespace gestalt::application {

    void LightSystem::prepare() {

      LightBuffers light_data;

      descriptor_layout_builder_->clear();
      light_data.descriptor_layout
          = descriptor_layout_builder_->add_binding(15, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                  VK_SHADER_STAGE_FRAGMENT_BIT,
                              false, getMaxDirectionalLights())
            .add_binding(16, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT,
                             false, getMaxPointLights())
            .add_binding(17, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, false,
                             getMaxLights())
            .build(gpu_->getDevice());

      light_data.descriptor_set
          = resource_manager_->get_descriptor_pool()->allocate(gpu_->getDevice(), light_data.descriptor_layout);

      light_data.dir_light_buffer = resource_manager_->create_buffer(
          sizeof(GpuDirectionalLight) * getMaxDirectionalLights(),
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
          VMA_MEMORY_USAGE_CPU_TO_GPU);
      light_data.point_light_buffer = resource_manager_->create_buffer(
          sizeof(GpuPointLight) * getMaxPointLights(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
          VMA_MEMORY_USAGE_CPU_TO_GPU);
      light_data.view_proj_matrices = resource_manager_->create_buffer(
          sizeof(glm::mat4) * getMaxLights(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
          VMA_MEMORY_USAGE_CPU_TO_GPU);

      writer_->clear();
      std::vector<VkDescriptorBufferInfo> dirBufferInfos;
      for (int i = 0; i < getMaxDirectionalLights(); ++i) {
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = light_data.dir_light_buffer.buffer;
        bufferInfo.offset = 32 * i;
        bufferInfo.range = 32;
        dirBufferInfos.push_back(bufferInfo);
      }
      writer_->write_buffer_array(15, dirBufferInfos, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);

      std::vector<VkDescriptorBufferInfo> pointBufferInfos;
      for (int i = 0; i < getMaxPointLights(); ++i) {
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = light_data.point_light_buffer.buffer;
        bufferInfo.offset = 32 * i;
        bufferInfo.range = 32;
        pointBufferInfos.push_back(bufferInfo);
      }
      writer_->write_buffer_array(16, pointBufferInfos, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);

      std::vector<VkDescriptorBufferInfo> lightViewProjBufferInfos;
      for (int i = 0; i < getMaxLights(); ++i) {
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = light_data.view_proj_matrices.buffer;
        bufferInfo.offset = 64 * i;
        bufferInfo.range = 64;
        lightViewProjBufferInfos.push_back(bufferInfo);
      }
      writer_->write_buffer_array(17, lightViewProjBufferInfos, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
      writer_->update_set(gpu_->getDevice(), light_data.descriptor_set);

      repository_->register_buffer(light_data);
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

    void LightSystem::update_directional_lights(
        std::unordered_map<Entity, LightComponent>& lights) {
      auto& light_data = repository_->get_buffer<LightBuffers>();

      repository_->directional_lights.clear();
      for (auto& light_source : lights) {
        auto& [entity, light] = light_source;
        if (light.type != LightType::kDirectional) {
          continue;
        }

        auto& rotation = repository_->transform_components.get(entity).value().get().rotation;
        glm::vec3 direction = normalize(glm::vec3(0, 0, -1.f) * rotation);

        auto& view_proj
            = repository_->light_view_projections.get(light.light_view_projections.at(0));
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
      VK_CHECK(vmaMapMemory(gpu_->getAllocator(), light_data.dir_light_buffer.allocation, &mapped_data));
      memcpy(mapped_data, repository_->directional_lights.data().data(),
             sizeof(GpuDirectionalLight) * repository_->directional_lights.size());
      vmaUnmapMemory(gpu_->getAllocator(), light_data.dir_light_buffer.allocation);
    }

    void LightSystem::update_point_lights(std::unordered_map<Entity, LightComponent>& lights) {
      auto& light_data = repository_->get_buffer<LightBuffers>();

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
      VK_CHECK(
          vmaMapMemory(gpu_->getAllocator(), light_data.point_light_buffer.allocation, &mapped_data));
      memcpy(mapped_data, repository_->point_lights.data().data(),
             sizeof(GpuPointLight) * repository_->point_lights.size());
      vmaUnmapMemory(gpu_->getAllocator(), light_data.point_light_buffer.allocation);
    }

    void LightSystem::update() {
      auto& light_components = repository_->light_components.components();
      if (has_dirty_light(light_components)) {
        update_directional_lights(light_components);
        update_point_lights(light_components);
      }

      // changes in the scene graph can affect the light view-projection matrices
      const auto& light_data = repository_->get_buffer<LightBuffers>();
      const auto& matrices = repository_->light_view_projections.data();
      void* mapped_data;
      VK_CHECK(
          vmaMapMemory(gpu_->getAllocator(), light_data.view_proj_matrices.allocation, &mapped_data));
      memcpy(mapped_data, matrices.data(), sizeof(glm::mat4) * matrices.size());
      vmaUnmapMemory(gpu_->getAllocator(), light_data.view_proj_matrices.allocation);
    }

    void LightSystem::cleanup() {

      repository_->directional_lights.clear();
      repository_ ->point_lights.clear();
      repository_ ->light_view_projections.clear();

      const auto& [dir_light_buffer, point_light_buffer, view_proj_matrices, light_set, light_layout] = repository_->get_buffer<LightBuffers>();
      resource_manager_->destroy_buffer(dir_light_buffer);
      resource_manager_->destroy_buffer(point_light_buffer);
      resource_manager_->destroy_buffer(view_proj_matrices);

      if (light_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(gpu_->getDevice(), light_layout, nullptr);
      }

      repository_->deregister_buffer<LightBuffers>();
    }

}  // namespace gestalt