#include "LightSystem.hpp"

#include "FrameProvider.hpp"

#include "VulkanCheck.hpp"
#include "Camera/AnimationCameraData.hpp"
#include "Interface/IDescriptorLayoutBuilder.hpp"
#include "Interface/IGpu.hpp"
#include "Interface/IResourceAllocator.hpp"
#include "Interface/IResourceManager.hpp"
#include "Resources/GpuProjViewData.hpp"

namespace gestalt::application {

  inline size_t GetMaxViewProjMatrices() { return getMaxPointLights() * 6 + getMaxDirectionalLights(); }

  void LightSystem::prepare() {
      create_buffers();
    fill_buffers();
  }

  void LightSystem::create_buffers() {
    const auto& light_data = repository_->light_buffers;

    descriptor_layout_builder_->clear();
    const auto descriptor_layout
        = descriptor_layout_builder_
              ->add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT
                                | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_COMPUTE_BIT
                                | VK_SHADER_STAGE_FRAGMENT_BIT)
              .add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT
                               | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_COMPUTE_BIT
                               | VK_SHADER_STAGE_FRAGMENT_BIT)
              .add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT
                               | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_COMPUTE_BIT
                               | VK_SHADER_STAGE_FRAGMENT_BIT)
              .build(gpu_->getDevice(), VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);


    light_data->descriptor_buffer = resource_manager_->create_descriptor_buffer(
        descriptor_layout, 3, 0);
    vkDestroyDescriptorSetLayout(gpu_->getDevice(), descriptor_layout, nullptr);

    // for each directional light we need a view and a projection matrix
    light_data->view_proj_matrices = resource_manager_->create_buffer(
        sizeof(GpuProjViewData) * GetMaxViewProjMatrices(),
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

    BufferTemplate buffer_template(
        "Light Buffer", sizeof(GpuDirectionalLight) * getMaxDirectionalLights(),
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU);
    light_data->light_buffer = resource_allocator_->create_buffer(std::move(buffer_template));

  }

  void LightSystem::fill_buffers() {
    const auto& light_data = repository_->light_buffers;

    light_data->descriptor_buffer
        ->write_buffer(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                       light_data->view_proj_matrices->address,
                       sizeof(GpuProjViewData) * GetMaxViewProjMatrices())
        .write_buffer(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, light_data->dir_light_buffer->address,
                      sizeof(GpuDirectionalLight) * getMaxDirectionalLights())
        .write_buffer(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, light_data->point_light_buffer->address,
                      sizeof(GpuPointLight) * getMaxPointLights())
        .update();
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

  glm::mat4 LightSystem::calculate_directional_light_proj_matrix(const glm::mat4& light_view) const {

    const glm::vec4 ndc_corners[8] = {
      {-1, -1, 0, 1},
      {1, -1, 0, 1},
      {1, 1, 0, 1},
      {-1, 1, 0, 1},
      {-1, -1, 1, 1},
      {1, -1, 1, 1},
      {1, 1, 1, 1},
      {-1, 1, 1, 1}};

    // Compute the corners of the camera frustum in world space
    // TODO uses last frames inv_viewProj, should be updated to use the current frame
    glm::mat4 inv_cam = repository_->per_frame_data_buffers.get()
                            ->data.at(frame_->get_current_frame_index())
                            .inv_viewProj;
    glm::vec3 world_corners[8];
    for (int i = 0; i < 8; ++i) {
      glm::vec4 world_pos = inv_cam * ndc_corners[i];
      world_corners[i] = glm::vec3(world_pos) / world_pos.w;
    }

    // extend the frustum to include the scene bounds
    // this ensures that the shadow map covers the entire scene, even occluders that are outside the
    // view frustum
    auto& [scene_min, scene_max, dirty] = repository_->scene_graph.get(root_entity).value().get().bounds;
    glm::vec3 extended_min = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 extended_max = glm::vec3(std::numeric_limits<float>::lowest());

    for (auto world_corner : world_corners) {
      extended_min = glm::min(extended_min, world_corner);
      extended_max = glm::max(extended_max, world_corner);
    }

    extended_min = glm::min(extended_min, scene_min);
    extended_max = glm::max(extended_max, scene_max);

    // transform the corners to light space
    glm::mat4 light_view_inv = glm::inverse(light_view);

    glm::vec3 light_space_min_corner = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 light_space_max_corner = glm::vec3(std::numeric_limits<float>::lowest());

    for (glm::vec3 world_corners[8] = {
             extended_min,                                              // Bottom-left-near
             glm::vec3(extended_max.x, extended_min.y, extended_min.z), // Bottom-right-near
             glm::vec3(extended_min.x, extended_max.y, extended_min.z), // Top-left-near
             glm::vec3(extended_max.x, extended_max.y, extended_min.z), // Top-right-near
             glm::vec3(extended_min.x, extended_min.y, extended_max.z), // Bottom-left-far
             glm::vec3(extended_max.x, extended_min.y, extended_max.z), // Bottom-right-far
             glm::vec3(extended_min.x, extended_max.y, extended_max.z), // Top-left-far
             extended_max                                               // Top-right-far
         }; auto world_corner : world_corners) {
      glm::vec4 light_pos = light_view_inv * glm::vec4(world_corner, 1.0);
      light_space_min_corner = glm::min(light_space_min_corner, glm::vec3(light_pos));
      light_space_max_corner = glm::max(light_space_max_corner, glm::vec3(light_pos));
    }

    return glm::orthoRH_ZO(light_space_min_corner.x, light_space_max_corner.x,  // Left, right
                           light_space_min_corner.y, light_space_max_corner.y,  // Bottom, top
                           light_space_min_corner.z, light_space_max_corner.z   // Near, far
    );

    /*
    const glm::vec3 ndc_test = lightProjection * lightView * glm::vec4(center, 1.0f);
    assert(ndc_test.x  >= 0 && ndc_test.x <= 1 && ndc_test.y >= 0 && ndc_test.y <= 1 && ndc_test.z
    >= 0 && ndc_test.z <= 1);*/
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
        auto proj = calculate_directional_light_proj_matrix(view);
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
           sizeof(GpuProjViewData) * repository_->light_view_projections.size());
    vmaUnmapMemory(gpu_->getAllocator(), light_data->view_proj_matrices->allocation);

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