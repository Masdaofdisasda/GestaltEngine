#include "SceneSystem.h"

#include <glm/detail/_noise.hpp>

namespace gestalt {
  namespace application {

    using namespace foundation;

    void LightSystem::prepare() {
      auto& light_data = resource_manager_->light_data;

      light_data.light_set
          = resource_manager_->descriptorPool.allocate(gpu_.device, light_data.light_layout);

      constexpr size_t max_directional_lights = kLimits.max_directional_lights;
      constexpr size_t max_point_lights = kLimits.max_point_lights;
      constexpr size_t max_lights = max_directional_lights + max_point_lights;

      light_data.dir_light_buffer = resource_manager_->create_buffer(
          sizeof(GpuDirectionalLight) * max_directional_lights, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
          VMA_MEMORY_USAGE_CPU_TO_GPU);
      light_data.point_light_buffer = resource_manager_->create_buffer(
          sizeof(GpuPointLight) * max_point_lights, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
          VMA_MEMORY_USAGE_CPU_TO_GPU);
      light_data.view_proj_matrices = resource_manager_->create_buffer(
          sizeof(glm::mat4) * max_lights, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
          VMA_MEMORY_USAGE_CPU_TO_GPU);

      writer_.clear();
      std::vector<VkDescriptorBufferInfo> dirBufferInfos;
      for (int i = 0; i < max_directional_lights; ++i) {
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = resource_manager_->light_data.dir_light_buffer.buffer;
        bufferInfo.offset = 32 * i;
        bufferInfo.range = 32;
        dirBufferInfos.push_back(bufferInfo);
      }
      writer_.write_buffer_array(15, dirBufferInfos, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);

      std::vector<VkDescriptorBufferInfo> pointBufferInfos;
      for (int i = 0; i < max_point_lights; ++i) {
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = resource_manager_->light_data.point_light_buffer.buffer;
        bufferInfo.offset = 32 * i;
        bufferInfo.range = 32;
        pointBufferInfos.push_back(bufferInfo);
      }
      writer_.write_buffer_array(16, pointBufferInfos, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);

      std::vector<VkDescriptorBufferInfo> lightViewProjBufferInfos;
      for (int i = 0; i < max_lights; ++i) {
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = resource_manager_->light_data.view_proj_matrices.buffer;
        bufferInfo.offset = 64 * i;
        bufferInfo.range = 64;
        lightViewProjBufferInfos.push_back(bufferInfo);
      }
      writer_.write_buffer_array(17, lightViewProjBufferInfos, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
      writer_.update_set(gpu_.device, resource_manager_->light_data.light_set);
    }

    void MaterialSystem::write_material(PbrMaterial& material, const uint32_t material_id) {
      writer_.clear();

      std::vector<VkDescriptorImageInfo> imageInfos = {
          {material.textures.albedo_sampler, material.textures.albedo_image.imageView,
           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
          {material.textures.metal_rough_sampler, material.textures.metal_rough_image.imageView,
           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
          {material.textures.normal_sampler, material.textures.normal_image.imageView,
           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
          {material.textures.emissive_sampler, material.textures.emissive_image.imageView,
           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
          {material.textures.occlusion_sampler, material.textures.occlusion_image.imageView,
           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};

      const uint32_t texture_start = imageInfos.size() * material_id;
      writer_.write_image_array(4, imageInfos, texture_start);

      writer_.update_set(gpu_.device, repository_->material_data.resource_set);

      if (material.constants.albedo_tex_index != kUnusedTexture) {
        material.constants.albedo_tex_index = texture_start;
      }
      if (material.constants.metal_rough_tex_index != kUnusedTexture) {
        material.constants.metal_rough_tex_index = texture_start + 1;
      }
      if (material.constants.normal_tex_index != kUnusedTexture) {
        material.constants.normal_tex_index = texture_start + 2;
      }
      if (material.constants.emissive_tex_index != kUnusedTexture) {
        material.constants.emissive_tex_index = texture_start + 3;
      }
      if (material.constants.occlusion_tex_index != kUnusedTexture) {
        material.constants.occlusion_tex_index = texture_start + 4;
      }

      PbrMaterial::PbrConstants* mappedData;
      vmaMapMemory(gpu_.allocator, repository_->material_data.constants_buffer.allocation,
                   (void**)&mappedData);
      memcpy(mappedData + material_id, &material.constants, sizeof(PbrMaterial::PbrConstants));
      vmaUnmapMemory(gpu_.allocator, repository_->material_data.constants_buffer.allocation);
    }

    void MaterialSystem::create_defaults() {
      auto& default_material = repository_->default_material_;

      uint32_t white = 0xFFFFFFFF;              // White color for color and occlusion
      uint32_t default_metallic_roughness
          = 0xFF00FFFF;                         // Green color representing metallic-roughness
      uint32_t flat_normal = 0xFFFF8080;        // Flat normal
      uint32_t black = 0xFF000000;              // Black color for emissive

      default_material.color_image = resource_manager_->create_image(
          (void*)&white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
      repository_->textures.add(default_material.color_image);

      default_material.metallic_roughness_image
          = resource_manager_->create_image((void*)&default_metallic_roughness, VkExtent3D{1, 1, 1},
                         VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
      repository_->textures.add(default_material.metallic_roughness_image);

      default_material.normal_image
          = resource_manager_->create_image((void*)&flat_normal, VkExtent3D{1, 1, 1},
                                            VK_FORMAT_R8G8B8A8_UNORM,
                         VK_IMAGE_USAGE_SAMPLED_BIT);
      repository_->textures.add(default_material.normal_image);

      default_material.emissive_image = resource_manager_->create_image(
          (void*)&black, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
      repository_->textures.add(default_material.emissive_image);

      default_material.occlusion_image = resource_manager_->create_image(
          (void*)&white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
      repository_->textures.add(default_material.occlusion_image);

      // checkerboard image for error textures and testing
      uint32_t magenta = 0xFFFF00FF;
      constexpr size_t checkerboard_size = 256;
      std::array<uint32_t, checkerboard_size> pixels;  // for 16x16 checkerboard texture
      for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
          pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
      }
      default_material.error_checkerboard_image
          = resource_manager_->create_image(pixels.data(), VkExtent3D{16, 16, 1},
                                            VK_FORMAT_R8G8B8A8_UNORM,
                         VK_IMAGE_USAGE_SAMPLED_BIT);
      repository_->textures.add(default_material.error_checkerboard_image);

      VkSamplerCreateInfo sampler = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

      sampler.magFilter = VK_FILTER_NEAREST;
      sampler.minFilter = VK_FILTER_NEAREST;

      vkCreateSampler(gpu_.device, &sampler, nullptr, &default_material.nearestSampler);
      repository_->samplers.add(default_material.nearestSampler);

      sampler.magFilter = VK_FILTER_LINEAR;
      sampler.minFilter = VK_FILTER_LINEAR;
      vkCreateSampler(gpu_.device, &sampler, nullptr, &default_material.linearSampler);
      repository_->samplers.add(default_material.linearSampler);

      PbrMaterial pbr_material{};

      // default the material textures
      pbr_material.textures.albedo_image = default_material.color_image;
      pbr_material.textures.albedo_sampler = default_material.linearSampler;
      pbr_material.textures.metal_rough_image = default_material.metallic_roughness_image;
      pbr_material.textures.metal_rough_sampler = default_material.linearSampler;
      pbr_material.textures.normal_image = default_material.normal_image;
      pbr_material.textures.normal_sampler = default_material.linearSampler;
      pbr_material.textures.emissive_image = default_material.emissive_image;
      pbr_material.textures.emissive_sampler = default_material.linearSampler;
      pbr_material.textures.occlusion_image = default_material.occlusion_image;
      pbr_material.textures.occlusion_sampler = default_material.nearestSampler;

      {
        // build material
        const size_t material_id = repository_->materials.size();
        const std::string key = "default_material";
        write_material(pbr_material, material_id);
        repository_->materials.add(Material{.name = key, .config = pbr_material});
        fmt::print("creating material {}, mat_id {}\n", key, material_id);
      }

      std::vector<PbrMaterial::PbrConstants> material_constants(kLimits.max_materials);

      PbrMaterial::PbrConstants* mappedData;
      VK_CHECK(vmaMapMemory(gpu_.allocator, repository_->material_data.constants_buffer.allocation,
                            (void**)&mappedData));
      memcpy(mappedData, material_constants.data(),
             sizeof(PbrMaterial::PbrConstants) * kLimits.max_materials);

      vmaUnmapMemory(gpu_.allocator, repository_->material_data.constants_buffer.allocation);

      std::vector<VkDescriptorBufferInfo> bufferInfos;
      for (int i = 0; i < material_constants.size(); ++i) {
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = repository_->material_data.constants_buffer.buffer;
        bufferInfo.offset = sizeof(PbrMaterial::PbrConstants) * i;
        bufferInfo.range = sizeof(PbrMaterial::PbrConstants);
        bufferInfos.push_back(bufferInfo);
      }

      writer_.clear();
      writer_.write_buffer_array(5, bufferInfos, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
      writer_.update_set(gpu_.device, repository_->material_data.constants_set);

      writer_.clear();
      std::vector<VkDescriptorImageInfo> imageInfos{kLimits.max_textures};
      for (int i = 0; i < kLimits.max_textures; ++i) {
        VkDescriptorImageInfo image_info;
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        image_info.imageView = repository_->default_material_.error_checkerboard_image.imageView;
        image_info.sampler = repository_->default_material_.linearSampler;
        imageInfos[i] = image_info;
      }
      writer_.write_image_array(4, imageInfos, 0);
      writer_.update_set(gpu_.device, repository_->material_data.resource_set);
    }

    void MaterialSystem::prepare() {

      auto& material_data = repository_->material_data;

      material_data.constants_buffer
          = resource_manager_->create_buffer(sizeof(PbrMaterial::PbrConstants) * kLimits.max_materials,
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
      material_data.resource_layout
          = graphics::DescriptorLayoutBuilder()
            .add_binding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                         VK_SHADER_STAGE_FRAGMENT_BIT, true)
            .build(gpu_.device);
      material_data.constants_layout = graphics::DescriptorLayoutBuilder()
                                           .add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT, true)
                                           .build(gpu_.device);
      material_data.resource_set = resource_manager_->descriptorPool.allocate(
          gpu_.device, material_data.resource_layout, {kLimits.max_textures});
      material_data.constants_set = resource_manager_->descriptorPool.allocate(
          gpu_.device, material_data.constants_layout, {kLimits.max_materials});

      create_defaults();

      
      resource_manager_->load_and_process_cubemap(R"(..\..\assets\san_giuseppe_bridge_4k.hdr)");
    }

    void MaterialSystem::update() {
      for (size_t i = 0; i < repository_->materials.size(); ++i) {
        auto& [name, config, is_dirty] = repository_->materials.get(i);
            if (is_dirty) {
                write_material(config, i);
                is_dirty = false;
            }
        }
    }

    void MaterialSystem::cleanup() {
	  //destroy material buffers
    }

    glm::mat4 LightSystem::calculate_sun_view_proj(const glm::vec3 direction) const {
      auto& [min, max, is_dirty] = repository_->scene_graph.get(root_entity).value().get().bounds;

      glm::vec3 center = (min + max) * 0.5f;
      glm::vec3 size = max - min;
      float radius = glm::length(size) * 0.5f;
      glm::vec3 lightPosition = center + direction * radius;

      // Calculate the tight orthographic frustum
      float halfWidth = radius;       // Width of the frustum
      float halfHeight = radius;      // Height of the frustum
      float nearPlane = 0.1f;         // Near plane distance
      float farPlane = radius * 2.f;  // Far plane distance

      // Calculate the view matrix for the light
      glm::mat4 lightView = glm::lookAt(lightPosition, center, glm::vec3(0.0f, 1.0f, 0.0f));

      // Calculate the projection matrix for the light
      glm::mat4 lightProjection
          = glm::orthoRH_ZO(-halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane, farPlane);
      /*
      const glm::vec3 ndc_test = lightProjection * lightView * glm::vec4(center, 1.0f);
      assert(ndc_test.x  >= 0 && ndc_test.x <= 1 && ndc_test.y >= 0 && ndc_test.y <= 1 && ndc_test.z
      >= 0 && ndc_test.z <= 1);*/

      return lightProjection * lightView;
    }

    bool has_dirty_light(const std::unordered_map<entity, LightComponent>& lights) {
      return std::any_of(
          lights.begin(), lights.end(),
          [](const std::pair<entity, LightComponent>& light) { return light.second.is_dirty; });
    }

    void LightSystem::update_directional_lights(
        std::unordered_map<entity, LightComponent>& lights) {
      auto& light_data = resource_manager_->light_data;

      repository_->directional_lights.clear();
      for (auto& light_source : lights) {
        auto& [entity, light] = light_source;
        if (light.type != LightType::kDirectional) {
          continue;
        }

        auto& rotation = repository_->transform_components.get(entity).value().get().rotation;
        glm::vec3 direction = glm::normalize(glm::vec3(0, 0, -1.f) * rotation);

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
      VK_CHECK(vmaMapMemory(gpu_.allocator, light_data.dir_light_buffer.allocation, &mapped_data));
      memcpy(mapped_data, repository_->directional_lights.data().data(),
             sizeof(GpuDirectionalLight) * repository_->directional_lights.size());
      vmaUnmapMemory(gpu_.allocator, light_data.dir_light_buffer.allocation);
    }

    void LightSystem::update_point_lights(std::unordered_map<entity, LightComponent>& lights) {
      auto& light_data = resource_manager_->light_data;

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
          vmaMapMemory(gpu_.allocator, light_data.point_light_buffer.allocation, &mapped_data));
      memcpy(mapped_data, repository_->point_lights.data().data(),
             sizeof(GpuPointLight) * repository_->point_lights.size());
      vmaUnmapMemory(gpu_.allocator, light_data.point_light_buffer.allocation);
    }

    void LightSystem::update() {
      auto& light_components = repository_->light_components.components();
      if (has_dirty_light(light_components)) {
        update_directional_lights(light_components);
        update_point_lights(light_components);
      }

      // changes in the scene graph can affect the light view-projection matrices
      const auto& light_data = resource_manager_->light_data;
      const auto& matrices = repository_->light_view_projections.data();
      void* mapped_data;
      VK_CHECK(
          vmaMapMemory(gpu_.allocator, light_data.view_proj_matrices.allocation, &mapped_data));
      memcpy(mapped_data, matrices.data(), sizeof(glm::mat4) * matrices.size());
      vmaUnmapMemory(gpu_.allocator, light_data.view_proj_matrices.allocation);
    }

    void LightSystem::cleanup() {
      resource_manager_->destroy_buffer(resource_manager_->light_data.dir_light_buffer);
      resource_manager_->destroy_buffer(resource_manager_->light_data.point_light_buffer);
      resource_manager_->destroy_buffer(resource_manager_->light_data.view_proj_matrices);
    }


    void CameraSystem::prepare() {
        free_fly_camera_ = std::make_unique<FreeFlyCamera>();
      free_fly_camera_->init(glm::vec3(7, 1.8, -7), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        active_camera_ = std::make_unique<Camera>();
      active_camera_->init(*free_fly_camera_);
    }

    void CameraSystem::update_cameras(const float delta_time, const Movement& movement, float aspect) {
      aspect_ = aspect;
              active_camera_->update(delta_time, movement);
    }

    void CameraSystem::update() {
      glm::mat4 view = active_camera_->get_view_matrix();

      // camera projection
      glm::mat4 projection = glm::perspective(
          glm::radians(70.f), aspect_, 0.1f,
          1000.f);

      // invert the Y direction on projection matrix so that we are more similar
      // to opengl and gltf axis
      projection[1][1] *= -1;

      auto& camera = repository_->cameras.get(0);  // assume this as the main camera for now
      camera.view_matrix = view;
      camera.projection_matrix = projection;
    }

    void CameraSystem::cleanup() {
    }

    glm::mat4 TransformSystem::get_model_matrix(const TransformComponent& transform) {
      return translate(glm::mat4(1.0f), transform.position) * mat4_cast(transform.rotation)
             * scale(glm::mat4(1.0f), glm::vec3(transform.scale));
    }

    void TransformSystem::prepare() {}

    void TransformSystem::mark_parent_dirty(entity entity) {
      if (entity == invalid_entity) {
        return;
      }

      auto& node = repository_->scene_graph.get(entity).value().get();
      if (node.bounds.is_dirty) {
        return;
      }

      node.bounds.is_dirty = true;
      mark_parent_dirty(node.parent);
    }

    void TransformSystem::mark_children_dirty(entity entity) {
      if (entity == invalid_entity) {
        return;
      }

      auto& node = repository_->scene_graph.get(entity).value().get();
      if (node.bounds.is_dirty) {
        return;
      }

      node.bounds.is_dirty = true;
      for (const auto& child : node.children) {
        mark_children_dirty(child);
      }
    }

    void TransformSystem::mark_as_dirty(entity entity) {
      const auto& node = repository_->scene_graph.get(entity).value().get();
      node.bounds.is_dirty = true;
      for (const auto& child : node.children) {
        mark_children_dirty(child);
      }
      mark_parent_dirty(node.parent);
    }

    void TransformSystem::update_aabb(const entity entity, const glm::mat4& parent_transform) {
      auto& node = repository_->scene_graph.get(entity).value().get();
      if (!node.bounds.is_dirty) {
        return;
      }
      auto& transform = repository_->transform_components.get(entity).value().get();
      if (node.parent != invalid_entity) {
        const auto& parent_transform_component
            = repository_->transform_components.get(node.parent).value().get();
        transform.parent_position = parent_transform_component.position;
        transform.parent_rotation = parent_transform_component.rotation;
        transform.parent_scale = parent_transform_component.scale;
      } else {
        transform.parent_position = transform.position;
        transform.parent_rotation = transform.rotation;
        transform.parent_scale = transform.scale;
      }

      const auto& mesh_optional = repository_->mesh_components.get(entity);

      AABB aabb;
      auto& [min, max, is_dirty] = aabb;

      if (mesh_optional.has_value()) {
        const auto& mesh = repository_->meshes.get(mesh_optional->get().mesh);
        min.x = std::min(min.x, mesh.local_bounds.min.x);
        min.y = std::min(min.y, mesh.local_bounds.min.y);
        min.z = std::min(min.z, mesh.local_bounds.min.z);

        max.x = std::max(max.x, mesh.local_bounds.max.x);
        max.y = std::max(max.y, mesh.local_bounds.max.y);
        max.z = std::max(max.z, mesh.local_bounds.max.z);
      } else {
        // nodes without mesh components should still influence the bounds
        min = glm::vec3(-0.0001f);
        max = glm::vec3(0.0001f);
      }
      const glm::mat4 local_matrix = get_model_matrix(transform);
      const glm::mat4 model_matrix = parent_transform * local_matrix;
      // Decompose model_matrix into translation (T) and 3x3 rotation matrix (M)
      glm::vec3 T = glm::vec3(model_matrix[3]);  // Translation vector
      glm::mat3 M = glm::mat3(model_matrix);     // Rotation matrix
      M = transpose(M);  // Otherwise the AABB will be rotated in the wrong direction

      AABB transformedAABB = {T, T};  // Start with a zero-volume AABB at T

      // Applying Arvo's method to adjust AABB based on rotation and translation
      // NOTE: does not work for non-uniform scaling
      for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
          float a = M[i][j] * min[j];
          float b = M[i][j] * max[j];
          transformedAABB.min[i] += a < b ? a : b;
          transformedAABB.max[i] += a < b ? b : a;
        }
      }
      min = transformedAABB.min;
      max = transformedAABB.max;

      for (const auto& child : node.children) {
        update_aabb(child, model_matrix);
        const auto& child_aabb = repository_->scene_graph.get(child).value().get().bounds;
        min.x = std::min(min.x, child_aabb.min.x);
        min.y = std::min(min.y, child_aabb.min.y);
        min.z = std::min(min.z, child_aabb.min.z);

        max.x = std::max(max.x, child_aabb.max.x);
        max.y = std::max(max.y, child_aabb.max.y);
        max.z = std::max(max.z, child_aabb.max.z);
      }

      node.bounds = aabb;
      node.bounds.is_dirty = false;
    }

    void TransformSystem::update() {
      for (auto& [entity, transform] : repository_->transform_components.components()) {
        if (transform.is_dirty) {
          mark_as_dirty(entity);
          repository_->model_matrices.set(transform.matrix, get_model_matrix(transform));
          transform.is_dirty = false;
        }
      }

      constexpr auto root_transform = glm::mat4(1.0f);

      update_aabb(root_entity, root_transform);

      // mark the directional light as dirty to update the view-projection matrix
      repository_->light_components.asVector().at(0).second.get().is_dirty = true;
    }

    void TransformSystem::cleanup() {}

    void RenderSystem::traverse_scene(const entity entity, const glm::mat4& parent_transform) {
      assert(entity != invalid_entity);
      const auto& node = repository_->scene_graph.get(entity).value().get();
      if (!node.visible) {
        return;
      }

      const auto& transform = repository_->transform_components.get(entity)->get();
      const glm::mat4 world_transform  // TODO check if matrix vector is needed
          = parent_transform * repository_->model_matrices.get(transform.matrix);

      const auto& mesh_component = repository_->mesh_components.get(entity);
      if (mesh_component.has_value()) {
        const auto& mesh = repository_->meshes.get(mesh_component->get().mesh);

        for (const auto surface : mesh.surfaces) {
          const auto& material = repository_->materials.get(surface.material);

          RenderObject def;
          def.index_count = surface.index_count;
          def.first_index = surface.first_index;
          def.vertex_offset = surface.vertex_offset;
          def.material = surface.material;
          def.transform = world_transform;

          if (material.config.transparent) {
            repository_->main_draw_context_.transparent_surfaces.push_back(def);
          } else {
            repository_->main_draw_context_.opaque_surfaces.push_back(def);
          }
        }
      }

      for (const auto& childEntity : node.children) {
        traverse_scene(childEntity, world_transform);
      }
    }

    void RenderSystem::prepare() {
      const size_t initial_vertex_position_buffer_size = 184521 * sizeof(GpuVertexPosition);
      const size_t initial_vertex_data_buffer_size = 184521 * sizeof(GpuVertexData);
      const size_t initial_index_buffer_size = 786801 * sizeof(uint32_t);

      // Create initially empty vertex buffer
      resource_manager_->scene_geometry_.vertexPositionBuffer = resource_manager_->create_buffer(
          initial_vertex_position_buffer_size,
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
              | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
          VMA_MEMORY_USAGE_GPU_ONLY);
      resource_manager_->scene_geometry_.vertexDataBuffer = resource_manager_->create_buffer(
          initial_vertex_data_buffer_size,
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
              | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
          VMA_MEMORY_USAGE_GPU_ONLY);

      // Create initially empty index buffer
      resource_manager_->scene_geometry_.indexBuffer = resource_manager_->create_buffer(
          initial_index_buffer_size,
          VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
          VMA_MEMORY_USAGE_GPU_ONLY);
    }

    void RenderSystem::update() {
      if (repository_->meshes.size() != meshes_) {
        meshes_ = repository_->meshes.size();

        /* TODO use this
        resource_manager_->update_mesh(resource_manager_->get_database().get_indices(),
                                       resource_manager_->get_database().get_vertices());*/
        resource_manager_->upload_mesh();
      }

      constexpr auto root_transform = glm::mat4(1.0f);

      traverse_scene(0, root_transform);
    }

    void HierarchySystem::prepare() {}

    void HierarchySystem::update() {}

    void HierarchySystem::cleanup() {}

  }  // namespace application
}  // namespace gestalt