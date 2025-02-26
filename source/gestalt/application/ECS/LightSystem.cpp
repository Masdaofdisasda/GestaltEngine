#include "LightSystem.hpp"

#include "FrameProvider.hpp"

#include "Camera/AnimationCameraData.hpp"
#include "Interface/IGpu.hpp"
#include "Interface/IResourceAllocator.hpp"
#include "Resources/GpuProjViewData.hpp"
#include "Resources/GpuSpotLight.hpp"

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
    light_data->spot_light_buffer
        = resource_allocator_->create_buffer(std::move(BufferTemplate(
            "Spot Light Storage Buffer", sizeof(GpuSpotLight) * getMaxSpotLights(),
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

  glm::mat4 LightSystem::calculate_directional_light_view_matrix_cam_only(const glm::vec3 direction, glm::mat4 cam_inv) const {
    glm::vec4 ndc_corners[8] = {
        {-1.0f, -1.0f, 0.0f, 1.0f},  // Near bottom-left
        {1.0f, -1.0f, 0.0f, 1.0f},   // Near bottom-right
        {1.0f, 1.0f, 0.0f, 1.0f},    // Near top-right
        {-1.0f, 1.0f, 0.0f, 1.0f},   // Near top-left
        {-1.0f, -1.0f, 1.0f, 1.0f},  // Far bottom-left
        {1.0f, -1.0f, 1.0f, 1.0f},   // Far bottom-right
        {1.0f, 1.0f, 1.0f, 1.0f},    // Far top-right
        {-1.0f, 1.0f, 1.0f, 1.0f},   // Far top-left
    };
    glm::vec3 frustum_corners_world[8];

    for (int i = 0; i < 8; ++i) {
      glm::vec4 world_pos = cam_inv * ndc_corners[i];
      frustum_corners_world[i] = glm::vec3(world_pos) / world_pos.w;  // Perspective divide
    }

    glm::vec3 frustum_center = glm::vec3(0.0f);
    for (auto i : frustum_corners_world) {
      frustum_center += i;
    }
    frustum_center /= 8.0f;

    const glm::vec3 lightDirection = glm::normalize(-direction);
    const glm::vec3 lightPosition = frustum_center - lightDirection;

    glm::vec3 up = glm::vec3(0.f, 1.f, 0.f);
    if (glm::abs(glm::dot(up, lightDirection)) > 0.999f) {
      up = glm::vec3(0.f, 0.f, 1.f);  // Avoid degenerate "up" vector
    }

    return glm::lookAt(lightPosition, frustum_center, up);
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
      const glm::mat4& light_view, const glm::mat4& inv_cam, float ndc_min_z /* = -1.f */,
      float shadowMapResolution  = 4 * 2048.f , 
      float zMult = 10.f )                  
  {
    //
    // 1. Transform an extended set of NDC corners to world space.
    //    Note: We allow NDC z from ndc_min_z to +1.0.
    //
    glm::vec4 ndc_corners[8] = {
        // Extended near plane
        {-1.f, -1.f, ndc_min_z, 1.f},
        {1.f, -1.f, ndc_min_z, 1.f},
        {1.f, 1.f, ndc_min_z, 1.f},
        {-1.f, 1.f, ndc_min_z, 1.f},

        // Far plane
        {-1.f, -1.f, 1.f, 1.f},
        {1.f, -1.f, 1.f, 1.f},
        {1.f, 1.f, 1.f, 1.f},
        {-1.f, 1.f, 1.f, 1.f},
    };

    glm::vec3 frustum_corners_world[8];
    for (int i = 0; i < 8; ++i) {
      glm::vec4 world_pos = inv_cam * ndc_corners[i];
      frustum_corners_world[i] = glm::vec3(world_pos) / world_pos.w;
    }

    //
    // 2. Compute an axis-aligned bounding box (AABB) for these corners in *light-view* space
    //
    glm::vec3 light_space_min(std::numeric_limits<float>::max());
    glm::vec3 light_space_max(std::numeric_limits<float>::lowest());

    for (int i = 0; i < 8; ++i) {
      glm::vec4 light_pos = light_view * glm::vec4(frustum_corners_world[i], 1.0f);
      glm::vec3 lpos3 = glm::vec3(light_pos);

      light_space_min = glm::min(light_space_min, lpos3);
      light_space_max = glm::max(light_space_max, lpos3);
    }

    //
    // 3. Optionally extend the light-space bounding box a bit to avoid edge clipping
    //    or behind-camera geometry that still casts shadows.
    //
    //    (Example: we add 10% padding in x/y, and expand the z range.)
    //
    glm::vec3 diag = light_space_max - light_space_min;
    glm::vec3 padding = diag * 0.1f;  // 10% padding
    light_space_min -= padding;
    light_space_max += padding;

    // Extend near/far planes to catch geometry well behind the camera if needed
    // Or to ensure we have some slack
    light_space_min.z -= (zMult * 1.0f);
    light_space_max.z += (zMult * 1.0f);

    //
    // 4. Snap the light-space bounding-box to the nearest texel to reduce shimmering.
    //
    //    The idea:
    //       1) figure out how big one texel is in world units
    //       2) shift your min/max (or center) to align with that grid
    //
    //    For example:
    //
    float worldUnitsPerTexelX = (light_space_max.x - light_space_min.x) / shadowMapResolution;
    float worldUnitsPerTexelY = (light_space_max.y - light_space_min.y) / shadowMapResolution;

    // We'll move the min corner so that (min.x / worldUnitsPerTexelX) is an integer, etc.
    auto snap = [&](float val, float size) { return floor(val / size) * size; };

    light_space_min.x = snap(light_space_min.x, worldUnitsPerTexelX);
    light_space_min.y = snap(light_space_min.y, worldUnitsPerTexelY);

    light_space_max.x = snap(light_space_max.x, worldUnitsPerTexelX);
    light_space_max.y = snap(light_space_max.y, worldUnitsPerTexelY);

    //
    // 5. Finally, build the orthographic projection from these light-space extents.
    //    Note we use a right-handed, zero-to-one clip-space version if your Vulkan setup expects
    //    it. If you use a different clip-space convention, adapt accordingly.
    //
    //    If you prefer left-handed or [-1,1] in Z, choose the appropriate glm::ortho* variant.
    //
    float left = light_space_min.x;
    float right = light_space_max.x;
    float bottom = light_space_min.y;
    float top = light_space_max.y;
    float nearZ = light_space_min.z;
    float farZ = light_space_max.z;

    return glm::orthoRH_ZO(left, right, bottom, top, nearZ, farZ);
  }


  void LightSystem::update(float delta_time, const UserInput& movement, float aspect) {
    repository_->light_view_projections.clear();
    repository_->directional_lights.clear();
    repository_->point_lights.clear();
    repository_->spot_lights.clear();

    for (auto [entity, Light_component] : repository_->light_components.asVector()) {
      if (Light_component.get().type == LightType::kDirectional) {
        auto& light = Light_component.get();
        const auto& rotation = repository_->transform_components[entity].rotation;
        auto& dir_light_data = std::get<DirectionalLightData>(Light_component.get().specific);
        glm::vec3 direction = -normalize(rotation * glm::vec3(0, 0, -1.f));

        dir_light_data.light_view_projection = repository_->light_view_projections.size();
        // TODO fix this
        glm::mat4 inv_cam
            = repository_->per_frame_data_buffers->data.at(frame_->get_current_frame_index())
                  .inv_viewProj;
        auto view = calculate_directional_light_view_matrix_cam_only(direction, inv_cam);
        auto proj = calculate_directional_light_proj_matrix_camera_only(view, inv_cam, 0.f);
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
      } else if (Light_component.get().type == LightType::kSpot) {
        auto& light = Light_component.get();
        const auto& position = repository_->transform_components[entity].position;
        const auto& rotation = repository_->transform_components[entity].rotation;
        const auto& spot_light_data = std::get<SpotLightData>(light.specific);
        GpuSpotLight spot_light = {};
        spot_light.color = light.base.color;
        spot_light.intensity = light.base.intensity;
        spot_light.position = position;
        spot_light.range = spot_light_data.range;
        spot_light.direction = normalize(rotation * glm::vec3(0, 0, -1.f));
        spot_light.inner_cone_angle = spot_light_data.inner_cone_cos;
        spot_light.outer_cone_angle = spot_light_data.outer_cone_cos;
        repository_->spot_lights.add(spot_light);
        light.is_dirty = false;
      }
    }

    auto& light_data = repository_->light_buffers;

    light_data->dir_light_buffer->copy_to_mapped(
        gpu_->getAllocator(), repository_->directional_lights.data().data(),
        sizeof(GpuDirectionalLight) * repository_->directional_lights.size());

    light_data->point_light_buffer->copy_to_mapped(
        gpu_->getAllocator(), repository_->point_lights.data().data(),
        sizeof(GpuPointLight) * repository_->point_lights.size());

    light_data->spot_light_buffer->copy_to_mapped(
        gpu_->getAllocator(), repository_->spot_lights.data().data(),
        sizeof(GpuSpotLight) * repository_->spot_lights.size());

    light_data->view_proj_matrices->copy_to_mapped(
        gpu_->getAllocator(), repository_->light_view_projections.data().data(),
        sizeof(GpuProjViewData) * repository_->light_view_projections.size());
  }

  void LightSystem::cleanup() {
    repository_->directional_lights.clear();
    repository_->point_lights.clear();
    repository_->spot_lights.clear();
    repository_->light_view_projections.clear();

    const auto& light_buffers = repository_->light_buffers;
    resource_allocator_->destroy_buffer(light_buffers->dir_light_buffer);
    resource_allocator_->destroy_buffer(light_buffers->point_light_buffer);
    resource_allocator_->destroy_buffer(light_buffers->view_proj_matrices);
    resource_allocator_->destroy_buffer(light_buffers->spot_light_buffer);

  }

}  // namespace gestalt::application