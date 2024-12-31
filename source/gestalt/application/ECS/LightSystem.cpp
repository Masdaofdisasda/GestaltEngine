#include "LightSystem.hpp"

#include "FrameProvider.hpp"

#include "VulkanCheck.hpp"
#include "Camera/AnimationCameraData.hpp"
#include "Interface/IGpu.hpp"
#include "Interface/IResourceAllocator.hpp"
#include "Interface/IResourceManager.hpp"
#include "Resources/GpuProjViewData.hpp"

namespace gestalt::application {

  inline size_t GetMaxViewProjMatrices() { return getMaxPointLights() * 6 + getMaxDirectionalLights(); }

  void LightSystem::prepare() {
      create_buffers();
  }

  void LightSystem::create_buffers() {
    const auto& light_data = repository_->light_buffers;

    light_data->dir_light_buffer
        = resource_allocator_->create_buffer(std::move(BufferTemplate(
            "Directional Light Storage Buffer", sizeof(GpuDirectionalLight) * getMaxDirectionalLights(),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VMA_MEMORY_USAGE_AUTO,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)));
    light_data->point_light_buffer
        = resource_allocator_->create_buffer(std::move(BufferTemplate(
            "Point Light Storage Buffer", sizeof(GpuPointLight) * getMaxPointLights(),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VMA_MEMORY_USAGE_AUTO,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)));
    light_data->view_proj_matrices
        = resource_allocator_->create_buffer(std::move(BufferTemplate(
            "View Proj Light Storage Buffer", sizeof(GpuProjViewData) * GetMaxViewProjMatrices(),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VMA_MEMORY_USAGE_AUTO,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)));

  }

  glm::mat4 LightSystem::calculate_directional_light_view_matrix(const glm::vec3 direction) const {
    auto& [min, max, is_dirty] = repository_->scene_graph.get(root_entity).value().get().bounds;

    const glm::vec3 center = (min + max) * 0.5f;
    glm::vec3 size = max - min;
    size = glm::max(size, glm::vec3(1.f));
    const float radius = length(size) * 0.5f;

    const glm::vec3 lightDirection = glm::normalize(-direction);
    const glm::vec3 lightPosition
        = center - lightDirection * radius;       // Position the light behind the scene

    glm::vec3 up = glm::vec3(0.f, 1.f, 0.f);
    if (dot(up, direction) < 0.0001f) {
      up = glm::vec3(0.f, 0.f, 1.f);
    }

    return glm::lookAt(lightPosition, center, up);
  }

  glm::mat4 LightSystem::calculate_directional_light_proj_matrix_scene_only(
      const glm::mat4& light_view) const {
    // Retrieve scene min/max
    auto& [scene_min, scene_max, dirty]
        = repository_->scene_graph.get(root_entity).value().get().bounds;

    // Step 1: Compute 8 corners of the scene bounding box
    glm::vec3 corners[8] = {scene_min,
                            {scene_max.x, scene_min.y, scene_min.z},
                            {scene_min.x, scene_max.y, scene_min.z},
                            {scene_max.x, scene_max.y, scene_min.z},
                            {scene_min.x, scene_min.y, scene_max.z},
                            {scene_max.x, scene_min.y, scene_max.z},
                            {scene_min.x, scene_max.y, scene_max.z},
                            scene_max};

    // Step 2: Transform corners into light space
    glm::mat4 light_view_inv = glm::inverse(light_view);
    glm::vec3 light_space_min = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 light_space_max = glm::vec3(std::numeric_limits<float>::lowest());

    for (const auto& c : corners) {
      glm::vec4 light_pos = light_view_inv * glm::vec4(c, 1.0f);
      glm::vec3 lpos3 = glm::vec3(light_pos);

      light_space_min = glm::min(light_space_min, lpos3);
      light_space_max = glm::max(light_space_max, lpos3);
    }

    // Step 3: Construct orthographic projection
    // Note: using orthoRH_ZO or orthoLH_ZO depends on your convention
    return glm::orthoRH_ZO(light_space_min.x, light_space_max.x, light_space_min.y,
                           light_space_max.y, light_space_min.z, light_space_max.z);
  }

  glm::mat4 LightSystem::calculate_directional_light_proj_matrix_camera_only(
      const glm::mat4& light_view, const glm::mat4& inv_cam,
      float ndc_min_z = -1.f) {
    // 1. Define NDC corners with extended behind-camera near plane
    //    Here, we let z range from ndc_min_z to 1.0f
    //    for a total of 8 corners.
    glm::vec4 ndc_corners[8] = {
        // near plane (extended behind camera)
        {-1.f, -1.f, ndc_min_z, 1.f},
        {1.f, -1.f, ndc_min_z, 1.f},
        {1.f, 1.f, ndc_min_z, 1.f},
        {-1.f, 1.f, ndc_min_z, 1.f},

        // far plane
        {-1.f, -1.f, 1.f, 1.f},
        {1.f, -1.f, 1.f, 1.f},
        {1.f, 1.f, 1.f, 1.f},
        {-1.f, 1.f, 1.f, 1.f},
    };

    // 2. Transform these NDC corners to world space via inv_cam
    glm::vec3 frustum_corners_world[8];
    for (int i = 0; i < 8; ++i) {
      glm::vec4 world_pos = inv_cam * ndc_corners[i];
      frustum_corners_world[i] = glm::vec3(world_pos) / world_pos.w;
    }

    // 3. Compute the bounding box in world space
    glm::vec3 extended_min = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 extended_max = glm::vec3(std::numeric_limits<float>::lowest());

    for (auto& corner : frustum_corners_world) {
      extended_min = glm::min(extended_min, corner);
      extended_max = glm::max(extended_max, corner);
    }

    // 4. Transform bounding box into light space
    glm::mat4 light_view_inv = glm::inverse(light_view);

    glm::vec3 light_space_min = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 light_space_max = glm::vec3(std::numeric_limits<float>::lowest());

    // Build the 8 corners of that bounding box
    glm::vec3 corners[8] = {extended_min,
                            {extended_max.x, extended_min.y, extended_min.z},
                            {extended_min.x, extended_max.y, extended_min.z},
                            {extended_max.x, extended_max.y, extended_min.z},
                            {extended_min.x, extended_min.y, extended_max.z},
                            {extended_max.x, extended_min.y, extended_max.z},
                            {extended_min.x, extended_max.y, extended_max.z},
                            extended_max};

    for (const auto& c : corners) {
      glm::vec4 light_pos = light_view_inv * glm::vec4(c, 1.0f);
      glm::vec3 lpos3 = glm::vec3(light_pos);

      light_space_min = glm::min(light_space_min, lpos3);
      light_space_max = glm::max(light_space_max, lpos3);
    }

    // 5. Construct the orthographic projection from these bounds
    return glm::orthoRH_ZO(light_space_min.x, light_space_max.x, light_space_min.y,
                           light_space_max.y, light_space_min.z, light_space_max.z);
  }

  void LightSystem::update(float delta_time, const UserInput& movement, float aspect) {

    repository_->light_view_projections.clear();
    repository_->directional_lights.clear();
    repository_->point_lights.clear();

    for (auto [entity, Light_component] : repository_->light_components.asVector()) {
      if (Light_component.get().type == LightType::kDirectional) {
        auto& light = Light_component.get();
        const auto& rotation = repository_->transform_components[entity].rotation;
        auto& dir_light_data = std::get<DirectionalLightData>(Light_component.get().specific);
        glm::vec3 direction = normalize(glm::vec3(0, 0, -1.f) * rotation);

        dir_light_data.light_view_projection = repository_->light_view_projections.size();
        auto view = calculate_directional_light_view_matrix(direction);
        // TODO fix this
        glm::mat4 inv_cam = repository_->per_frame_data_buffers
                                       ->data.at(frame_->get_current_frame_index())
                                       .inv_viewProj;
        auto proj = calculate_directional_light_proj_matrix_camera_only(view, inv_cam);
        repository_->light_view_projections.add({view, proj});

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
      VK_CHECK(vmaMapMemory(gpu_->getAllocator(), light_data->dir_light_buffer->get_allocation(),
                            &directional_light_data));
      memcpy(directional_light_data, repository_->directional_lights.data().data(),
             sizeof(GpuDirectionalLight) * repository_->directional_lights.size());
      vmaUnmapMemory(gpu_->getAllocator(), light_data->dir_light_buffer->get_allocation());

      void* point_light_data;
      VK_CHECK(vmaMapMemory(gpu_->getAllocator(), light_data->point_light_buffer->get_allocation(),
                            &point_light_data));
      memcpy(point_light_data, repository_->point_lights.data().data(),
             sizeof(GpuPointLight) * repository_->point_lights.size());
      vmaUnmapMemory(gpu_->getAllocator(),
                     light_data->point_light_buffer->get_allocation());

      // changes in the scene graph can affect the light view-projection matrices
      void* view_matrix_data;
      VK_CHECK(vmaMapMemory(gpu_->getAllocator(), light_data->view_proj_matrices->get_allocation(),
                            &view_matrix_data));
      memcpy(view_matrix_data, repository_->light_view_projections.data().data(),
             sizeof(GpuProjViewData) * repository_->light_view_projections.size());
      vmaUnmapMemory(gpu_->getAllocator(),
                     light_data->view_proj_matrices->get_allocation());
  }

  void LightSystem::cleanup() {
    repository_->directional_lights.clear();
    repository_->point_lights.clear();
    repository_->light_view_projections.clear();

    const auto& light_buffers = repository_->light_buffers;
    resource_allocator_->destroy_buffer(light_buffers->dir_light_buffer);
    resource_allocator_->destroy_buffer(light_buffers->point_light_buffer);
    resource_allocator_->destroy_buffer(light_buffers->view_proj_matrices);

  }

}  // namespace gestalt::application