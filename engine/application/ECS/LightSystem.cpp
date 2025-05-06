#include "LightSystem.hpp"

#include "FrameProvider.hpp"

#include "Components/DirectionalLightComponent.hpp"
#include "Components/PointLightComponent.hpp"
#include "Components/SpotLightComponent.hpp"
#include "Events/EventBus.hpp"
#include "Events/Events.hpp"
#include "Interface/IGpu.hpp"
#include "Interface/IResourceAllocator.hpp"
#include "Resources/GpuProjViewData.hpp"
#include "Resources/GpuSpotLight.hpp"

namespace gestalt::application {

  inline size_t GetMaxViewProjMatrices() { return getMaxPointLights() * 6 + getMaxDirectionalLights(); }

  
  LightSystem::LightSystem(IGpu& gpu, IResourceAllocator& resource_allocator,
                           Repository& repository, EventBus& event_bus, FrameProvider& frame)

      : gpu_(gpu),
        resource_allocator_(resource_allocator),
        repository_(repository),
        event_bus_(event_bus),
        frame_(frame) {
    event_bus_.subscribe<UpdateLightEvent>([this](const UpdateLightEvent& event) {
      auto dir_light = repository_.directional_light_components.find_mutable(event.entity());
      if (dir_light != nullptr) {
        dir_light->set_color(event.color());
        dir_light->set_intensity(event.intensity());
        dir_light->is_dirty = true;
        return;
      }
      auto point_light = repository_.point_light_components.find_mutable(event.entity());
      if (point_light != nullptr) {
        point_light->set_color(event.color());
        point_light->set_intensity(event.intensity());
        point_light->is_dirty = true;
        return;
      }
      auto spot_light = repository_.spot_light_components.find_mutable(event.entity());
      if (spot_light != nullptr) {
        spot_light->set_color(event.color());
        spot_light->set_intensity(event.intensity());
        spot_light->is_dirty = true;
      }
    });
    event_bus_.subscribe<UpdatePointLightEvent>([this](const UpdatePointLightEvent& event) {
      auto point_light = repository_.point_light_components.find_mutable(event.entity());
      if (point_light != nullptr) {
        point_light->set_range(event.range());
        point_light->is_dirty = true;
      }
    });
    event_bus_.subscribe<UpdateSpotLightEvent>([this](const UpdateSpotLightEvent& event) {
      auto spot_light = repository_.spot_light_components.find_mutable(event.entity());
      if (spot_light != nullptr) {
        spot_light->set_range(event.range());
        spot_light->set_inner_cone_cos(event.inner_cos());
        spot_light->set_outer_cone_cos(event.outer_cos());
        spot_light->is_dirty = true;
      }
    });
    create_buffers();
  }

  void LightSystem::create_buffers() {
    const auto& light_data = repository_.light_buffers;

    light_data->dir_light_buffer
        = resource_allocator_.create_buffer(std::move(BufferTemplate(
            "Directional Light Storage Buffer", sizeof(GpuDirectionalLight) * getMaxDirectionalLights(),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VMA_MEMORY_USAGE_AUTO,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)));
    light_data->point_light_buffer
        = resource_allocator_.create_buffer(std::move(BufferTemplate(
            "Point Light Storage Buffer", sizeof(GpuPointLight) * getMaxPointLights(),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VMA_MEMORY_USAGE_AUTO,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)));
    light_data->spot_light_buffer
        = resource_allocator_.create_buffer(std::move(BufferTemplate(
            "Spot Light Storage Buffer", sizeof(GpuSpotLight) * getMaxSpotLights(),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VMA_MEMORY_USAGE_AUTO,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)));
    light_data->view_proj_matrices
        = resource_allocator_.create_buffer(std::move(BufferTemplate(
            "View Proj Light Storage Buffer", sizeof(GpuProjViewData) * GetMaxViewProjMatrices(),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VMA_MEMORY_USAGE_AUTO,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)));

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

  void LightSystem::update() {
    repository_.light_view_projections.clear();
    repository_.directional_lights.clear();
    repository_.point_lights.clear();
    repository_.spot_lights.clear();

    for (auto [entity, light_component] : repository_.directional_light_components.snapshot()) {
        const auto& rotation = repository_.transform_components.find(entity)->rotation();
        glm::vec3 direction = -glm::normalize(rotation * glm::vec3(0, 0, -1.f));

        light_component.set_light_view_projection(static_cast<uint32>(repository_.light_view_projections.size()));
        // TODO fix this
        glm::mat4 inv_cam
            = repository_.per_frame_data_buffers->data.at(frame_.get_current_frame_index())
                  .inv_viewProj;
        auto view = calculate_directional_light_view_matrix_cam_only(direction, inv_cam);
        auto proj = calculate_directional_light_proj_matrix_camera_only(view, inv_cam, 0.f);
        repository_.light_view_projections.add({view, proj});

        GpuDirectionalLight dir_light = {};
        dir_light.color = light_component.color();
        dir_light.intensity = light_component.intensity();
        dir_light.direction = direction;
        dir_light.viewProj = light_component.light_view_projection();
        repository_.directional_lights.add(dir_light);

        light_component.is_dirty = false;
    }
    for (auto [entity, light_component] : repository_.point_light_components.snapshot()) {
        const auto& position = repository_.transform_components.find(entity)->position();

        // TODO Calculate the 6 view matrices for the light

        GpuPointLight point_light = {};
        point_light.color = light_component.color();
        point_light.intensity = light_component.intensity();
        point_light.position = position;
        point_light.range = light_component.range();
        repository_.point_lights.add(point_light);

        light_component.is_dirty = false;
    }
    for (auto [entity, light_component] : repository_.spot_light_components.snapshot()) {
        const auto& position = repository_.transform_components.find(entity)->position();
        const auto& rotation = repository_.transform_components.find(entity)->rotation();

        GpuSpotLight spot_light = {};
        spot_light.color = light_component.color();
        spot_light.intensity = light_component.intensity();
        spot_light.position = position;
        spot_light.range = light_component.range();
        spot_light.direction = glm::normalize(rotation * glm::vec3(0, 0, -1.f));
        spot_light.inner_cone_angle = light_component.inner_cone_cos();
        spot_light.outer_cone_angle = light_component.outer_cone_cos();
        repository_.spot_lights.add(spot_light);
        light_component.is_dirty = false;
    }

    auto& light_data = repository_.light_buffers;

    light_data->dir_light_buffer->copy_to_mapped(
        gpu_.getAllocator(), repository_.directional_lights.data().data(),
        sizeof(GpuDirectionalLight) * repository_.directional_lights.size());

    light_data->point_light_buffer->copy_to_mapped(
        gpu_.getAllocator(), repository_.point_lights.data().data(),
        sizeof(GpuPointLight) * repository_.point_lights.size());

    light_data->spot_light_buffer->copy_to_mapped(
        gpu_.getAllocator(), repository_.spot_lights.data().data(),
        sizeof(GpuSpotLight) * repository_.spot_lights.size());

    light_data->view_proj_matrices->copy_to_mapped(
        gpu_.getAllocator(), repository_.light_view_projections.data().data(),
        sizeof(GpuProjViewData) * repository_.light_view_projections.size());
  }

  LightSystem::~LightSystem() {
    repository_.directional_lights.clear();
    repository_.point_lights.clear();
    repository_.spot_lights.clear();
    repository_.light_view_projections.clear();

    const auto& light_buffers = repository_.light_buffers;
    resource_allocator_.destroy_buffer(light_buffers->dir_light_buffer);
    resource_allocator_.destroy_buffer(light_buffers->point_light_buffer);
    resource_allocator_.destroy_buffer(light_buffers->view_proj_matrices);
    resource_allocator_.destroy_buffer(light_buffers->spot_light_buffer);
  }

}  // namespace gestalt::application