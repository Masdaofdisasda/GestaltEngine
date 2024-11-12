#include "Renderpasses/RenderPassTypes.hpp"

#include <fmt/core.h>

#include "FrameProvider.hpp"
#include "ResourceManager.hpp"
#include "Interface/IGpu.hpp"
#include "Render Engine/ResourceRegistry.hpp"
#include "Utils/vk_descriptors.hpp"


namespace gestalt::graphics {

  struct alignas(16) VolumetricLightingInjectionPassConstants {
    glm::vec2 halton_xy{0.f,0.f}; // no jitter
    float32 temporal_reprojection_jitter_scale{0.f};
    float32 density_modifier{1.f};

    glm::uvec3 froxel_dimensions{128,128,128};
    int32 current_frame{0};

    float32 noise_scale{1.f};
    uint32 noise_type{0};
    float32 froxel_near{1.f};
    float32 froxel_far{100.f};

    glm::mat4 froxel_inverse_view_projection{1.f};

    float32 volumetric_noise_position_multiplier{1.f};
    float32 volumetric_noise_speed_multiplier{1.f};
    float32 height_fog_density{0.01f};
    float32 height_fog_falloff{1.f};

    glm::vec3 box_position{0.f};
    float32 box_fog_density{0.1f};

    glm::vec3 box_size{10.f};
    float32 scattering_factor{0.5f};
  };

  void VolumetricLightingInjectionPass::prepare() {
    fmt::print("Preparing {}\n", get_name());

    blue_noise_ = resource_manager_->load_image("../../assets/blue_noise_512_512.png").value();

    descriptor_layouts_.emplace_back(
        DescriptorLayoutBuilder()
            .add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          VK_SHADER_STAGE_COMPUTE_BIT )
            .add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                          VK_SHADER_STAGE_COMPUTE_BIT )
            .add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                          VK_SHADER_STAGE_COMPUTE_BIT )
            .build(gpu_->getDevice()));

    dependencies_ = RenderPassDependencyBuilder()
                        .add_shader(ShaderStage::kCompute, "volumetric_light_injection.comp.spv")
                        .add_image_attachment(registry_->resources_.froxel_data_texture_0, ImageUsageType::kStore)
                        .add_image_attachment(registry_->resources_.volumetric_noise_texture, ImageUsageType::kRead)
              .set_push_constant_range(sizeof(VolumetricLightingInjectionPassConstants),
                                       VK_SHADER_STAGE_COMPUTE_BIT)
              .build();

    create_pipeline_layout();

    pipeline_ = create_pipeline().build_compute_pipeline(gpu_->getDevice());
  }

  void VolumetricLightingInjectionPass::destroy() {
    resource_manager_->destroy_image(blue_noise_);
  }

  void VolumetricLightingInjectionPass::execute(VkCommandBuffer cmd) {

    const auto frame = frame_->get_current_frame_index();

    const auto volumetric_noise_texture = registry_->resources_.volumetric_noise_texture.image;
    const auto froxel_data_texture_0 = registry_->resources_.froxel_data_texture_0.image;

    if (descriptor_buffers_.at(frame) == nullptr) {
      descriptor_buffers_.at(frame)
          = resource_manager_->create_descriptor_buffer(descriptor_layouts_.at(0), 3, 0);
      auto image_info0 = VkDescriptorImageInfo{post_process_sampler, blue_noise_.imageView,
                                               blue_noise_.getLayout()};
      auto image_info1
          = VkDescriptorImageInfo{post_process_sampler, volumetric_noise_texture->imageView,
                                  volumetric_noise_texture->getLayout()};
      auto image_info2 = VkDescriptorImageInfo{post_process_sampler, froxel_data_texture_0->imageView,
                                  froxel_data_texture_0->getLayout()};
      descriptor_buffers_.at(frame)
          ->write_image(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info0)
          .write_image(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info1)
          .write_image(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, image_info2)
          .update();
    }

    bind_descriptor_buffers(
        cmd, {
              descriptor_buffers_.at(frame).get()});
    descriptor_buffers_.at(frame)->bind_descriptors(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                                    pipeline_layout_, 0);
    const auto dispatch_group_x = (uint32)ceil(froxel_data_texture_0->getExtent2D().width / 8.0f);
    const auto dispatch_group_y = (uint32)ceil(froxel_data_texture_0->getExtent2D().height / 8.0f);
    const auto dispatch_group_z = 128;
    
    begin_compute_pass(cmd);
    auto& perFrameData = repository_->per_frame_data_buffers.get()->data.at(frame);
    auto& params = registry_->config_.volumetric_lighting;
    VolumetricLightingInjectionPassConstants constants;
    constants.current_frame = frame_->get_current_frame_number();
    constants.froxel_inverse_view_projection
        = perFrameData.inv_viewProj;
    constants.density_modifier = params.density_modifier;
    constants.noise_scale = params.noise_scale;
    constants.noise_type = params.noise_type;
    constants.volumetric_noise_position_multiplier = params.volumetric_noise_position_multiplier;
    constants.froxel_near = perFrameData.znear;
    constants.froxel_far = perFrameData.zfar;
    constants.height_fog_density = params.height_fog_density;
    constants.height_fog_falloff = params.height_fog_falloff;
    constants.box_position = params.box_position;
    constants.box_fog_density = params.box_fog_density;
    constants.box_size = params.box_size;
    constants.scattering_factor = params.scattering_factor;

    vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(VolumetricLightingInjectionPassConstants), &constants);
    vkCmdDispatch(cmd, dispatch_group_x, dispatch_group_y, dispatch_group_z);

    VkMemoryBarrier memoryBarrier = {};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0,
                         nullptr);
  }
  
  struct alignas(16) VolumetricLightingScatteringPassConstants {
    glm::vec2 halton_xy{0.f, 0.f};  // no jitter
    float32 temporal_reprojection_jitter_scale{0.f};
    float32 density_modifier{1.f};

    glm::uvec3 froxel_dimensions{128, 128, 128};
    int32 current_frame{0};

    float32 noise_scale{1.f};
    uint32 noise_type{0};
    float32 froxel_near{1.f};
    float32 froxel_far{100.f};

    glm::mat4 froxel_inverse_view_projection{1.f};

    float32 scattering_factor{0.5f};
    float32 phase_anisotropy{0.2f};
    uint32 phase_type{0};
    uint32 num_point_lights;
  };


  void VolumetricLightingScatteringPass::prepare() {
    fmt::print("Preparing {}\n", get_name());

    blue_noise_ = resource_manager_->load_image("../../assets/blue_noise_512_512.png").value();

    descriptor_layouts_.emplace_back(
        DescriptorLayoutBuilder()
            .add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(gpu_->getDevice()));
    descriptor_layouts_.emplace_back(
        DescriptorLayoutBuilder()
            .add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(gpu_->getDevice()));
    descriptor_layouts_.emplace_back(
        DescriptorLayoutBuilder()
            .add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(gpu_->getDevice(), VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT));

    dependencies_ = RenderPassDependencyBuilder()
                        .add_shader(ShaderStage::kCompute, "volumetric_light_scattering.comp.spv")
                        .add_image_attachment(registry_->resources_.froxel_data_texture_0,
                                              ImageUsageType::kRead)
                        .add_image_attachment(registry_->resources_.shadow_map, ImageUsageType::kRead)
              .add_image_attachment(registry_->resources_.light_scattering_texture,
                                    ImageUsageType::kStore)
              .set_push_constant_range(sizeof(VolumetricLightingScatteringPassConstants),
                                                 VK_SHADER_STAGE_COMPUTE_BIT)
                        .build();

    create_pipeline_layout();

    pipeline_ = create_pipeline().build_compute_pipeline(gpu_->getDevice());
  }

  void VolumetricLightingScatteringPass::destroy() {
    resource_manager_->destroy_image(blue_noise_);
  }


  void VolumetricLightingScatteringPass::execute(VkCommandBuffer cmd) {
    const auto frame = frame_->get_current_frame_index();

    const auto froxel_data_texture_0 = registry_->resources_.froxel_data_texture_0.image;
    const auto shadow_map = registry_->resources_.shadow_map.image;
    const auto light_scattering = registry_->resources_.light_scattering_texture.image;

    const auto per_frame_buffers = repository_->per_frame_data_buffers.get();
    const auto light_data = repository_->light_buffers.get();

    if (descriptor_buffers_.at(frame) == nullptr) {
      descriptor_buffers_.at(frame)
          = resource_manager_->create_descriptor_buffer(descriptor_layouts_.at(1), 4, 0);
      auto image_info0 = VkDescriptorImageInfo{post_process_sampler, blue_noise_.imageView,
                                               blue_noise_.getLayout()};
      auto image_info1
          = VkDescriptorImageInfo{post_process_sampler, froxel_data_texture_0->imageView,
                                  froxel_data_texture_0->getLayout()};
      auto image_info2
          = VkDescriptorImageInfo{post_process_sampler, shadow_map->imageView,
                                  shadow_map->getLayout()};
      auto image_info3 = VkDescriptorImageInfo{post_process_sampler, light_scattering->imageView,
                                               light_scattering->getLayout()};
      descriptor_buffers_.at(frame)
          ->write_image(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info0)
          .write_image(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info1)
          .write_image(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info2)
          .write_image(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, image_info3)
          .update();
    }

    bind_descriptor_buffers(
        cmd, {per_frame_buffers->descriptor_buffers[frame].get(), descriptor_buffers_.at(frame).get(),
         light_data->descriptor_buffer.get(),
              descriptor_buffers_.at(frame).get()});

    per_frame_buffers->descriptor_buffers[frame]->bind_descriptors(
        cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout_, 0);
    descriptor_buffers_.at(frame)->bind_descriptors(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                                    pipeline_layout_, 1);
    light_data->descriptor_buffer->bind_descriptors(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                                    pipeline_layout_, 2);

    const auto dispatch_group_x = (uint32)ceil(light_scattering->getExtent2D().width / 8.0f);
    const auto dispatch_group_y = (uint32)ceil(light_scattering->getExtent2D().height / 8.0f);
    const auto dispatch_group_z = 128;

    begin_compute_pass(cmd);
    auto& perFrameData = repository_->per_frame_data_buffers.get()->data.at(frame);
    auto& params = registry_->config_.volumetric_lighting;
    VolumetricLightingScatteringPassConstants constants;
    constants.current_frame = frame_->get_current_frame_number();
    constants.froxel_inverse_view_projection = perFrameData.inv_viewProj;
    constants.froxel_near = perFrameData.znear;
    constants.froxel_far = perFrameData.zfar;
    constants.scattering_factor = params.scattering_factor;
    constants.phase_anisotropy = params.phase_anisotropy;
    constants.phase_type = params.phase_type;
    constants.density_modifier = params.density_modifier;
    constants.noise_scale = params.noise_scale;
    constants.noise_type = params.noise_type;
    constants.num_point_lights = repository_->point_lights.size();

    vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(VolumetricLightingScatteringPassConstants), &constants);
    vkCmdDispatch(cmd, dispatch_group_x, dispatch_group_y, dispatch_group_z);

    // TODO : Add barrier to ensure the compute shader has finished before the next pass
  }

  struct VolumetricLightingSpatialFilterPassConstants {
    glm::uvec3 froxel_dimensions{128, 128, 128};
    int32 use_spatial_filtering{1};
  };


  void VolumetricLightingSpatialFilterPass::prepare() {
    fmt::print("Preparing {}\n", get_name());

    descriptor_layouts_.emplace_back(
        DescriptorLayoutBuilder()
            .add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(gpu_->getDevice()));

    dependencies_
        = RenderPassDependencyBuilder()
              .add_shader(ShaderStage::kCompute, "volumetric_light_spatial_filter.comp.spv")
              .add_image_attachment(registry_->resources_.light_scattering_texture,
                                    ImageUsageType::kRead)
              .add_image_attachment(registry_->resources_.froxel_data_texture_0,
                                    ImageUsageType::kStore)
              .set_push_constant_range(sizeof(VolumetricLightingSpatialFilterPassConstants),
                                       VK_SHADER_STAGE_COMPUTE_BIT)
              .build();

    create_pipeline_layout();

    pipeline_ = create_pipeline().build_compute_pipeline(gpu_->getDevice());
  }

  void VolumetricLightingSpatialFilterPass::destroy() {}

  void VolumetricLightingSpatialFilterPass::execute(VkCommandBuffer cmd) {
    const auto frame = frame_->get_current_frame_index();

    const auto light_scattering_texture = registry_->resources_.light_scattering_texture.image;
    const auto froxel_data_texture_0 = registry_->resources_.froxel_data_texture_0.image;

    if (descriptor_buffers_.at(frame) == nullptr) {
      descriptor_buffers_.at(frame)
          = resource_manager_->create_descriptor_buffer(descriptor_layouts_.at(0), 2, 0);
      auto image_info0
          = VkDescriptorImageInfo{post_process_sampler, light_scattering_texture->imageView,
                                  light_scattering_texture->getLayout()};
      auto image_info1
          = VkDescriptorImageInfo{post_process_sampler, froxel_data_texture_0->imageView,
                                  froxel_data_texture_0->getLayout()};
      descriptor_buffers_.at(frame)
          ->write_image(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info0)
          .write_image(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, image_info1)
          .update();
    }

    bind_descriptor_buffers(cmd, {descriptor_buffers_.at(frame).get()});
    descriptor_buffers_.at(frame)->bind_descriptors(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                                    pipeline_layout_, 0);
    const auto dispatch_group_x = (uint32)ceil(froxel_data_texture_0->getExtent2D().width / 8.0f);
    const auto dispatch_group_y = (uint32)ceil(froxel_data_texture_0->getExtent2D().height / 8.0f);
    const auto dispatch_group_z = 128;

    begin_compute_pass(cmd);
    VolumetricLightingSpatialFilterPassConstants constants;
    constants.use_spatial_filtering = registry_->config_.volumetric_lighting.enable_spatial_filter;
    vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(VolumetricLightingSpatialFilterPassConstants), &constants);
    vkCmdDispatch(cmd, dispatch_group_x, dispatch_group_y, dispatch_group_z);
  }

  struct alignas(16) VolumetricLightingIntegrationPassConstants {
    glm::uvec3 froxel_dimensions{128, 128, 128};
    int32 current_frame{0};

    float32 noise_scale{1.f};
    uint32 noise_type{0};
    float32 froxel_near{1.f};
    float32 froxel_far{100.f};
  };

  void VolumetricLightingIntegrationPass::prepare() {
    fmt::print("Preparing {}\n", get_name());

    descriptor_layouts_.emplace_back(
        DescriptorLayoutBuilder()
            .add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(gpu_->getDevice()));

    dependencies_
        = RenderPassDependencyBuilder()
              .add_shader(ShaderStage::kCompute, "volumetric_light_integration.comp.spv")
              .add_image_attachment(registry_->resources_.froxel_data_texture_0,
                                    ImageUsageType::kRead)
              .add_image_attachment(registry_->resources_.integrated_light_scattering_texture,
                                    ImageUsageType::kStore)
                        .set_push_constant_range(sizeof(VolumetricLightingIntegrationPassConstants),
                                       VK_SHADER_STAGE_COMPUTE_BIT)
              .build();

    create_pipeline_layout();

    pipeline_ = create_pipeline().build_compute_pipeline(gpu_->getDevice());
  }

  void VolumetricLightingIntegrationPass::destroy() {}

  void VolumetricLightingIntegrationPass::execute(VkCommandBuffer cmd) {
    const auto frame = frame_->get_current_frame_index();

    const auto light_scattering_texture = registry_->resources_.froxel_data_texture_0.image;
    const auto integrated_light_scattering_texture
        = registry_->resources_.integrated_light_scattering_texture.image;

    if (descriptor_buffers_.at(frame) == nullptr) {
      descriptor_buffers_.at(frame)
          = resource_manager_->create_descriptor_buffer(descriptor_layouts_.at(0), 2, 0);
      auto image_info0
          = VkDescriptorImageInfo{post_process_sampler, light_scattering_texture->imageView,
                                  light_scattering_texture->getLayout()};
      auto image_info1 = VkDescriptorImageInfo{post_process_sampler,
                                               integrated_light_scattering_texture->imageView,
                                               integrated_light_scattering_texture->getLayout()};
      descriptor_buffers_.at(frame)
          ->write_image(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info0)
          .write_image(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, image_info1)
          .update();
    }

    bind_descriptor_buffers(cmd, {descriptor_buffers_.at(frame).get()});
    descriptor_buffers_.at(frame)->bind_descriptors(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                                    pipeline_layout_, 0);
    const auto dispatch_group_x
        = (uint32)ceil(integrated_light_scattering_texture->getExtent2D().width / 8.0f);
    const auto dispatch_group_y
        = (uint32)ceil(integrated_light_scattering_texture->getExtent2D().height / 8.0f);
    const auto dispatch_group_z = 1;

    begin_compute_pass(cmd);
    auto& perFrameData = repository_->per_frame_data_buffers.get()->data.at(frame);
    VolumetricLightingIntegrationPassConstants constants;
    constants.current_frame = frame_->get_current_frame_number();
    constants.froxel_near = perFrameData.znear;
    constants.froxel_far = perFrameData.zfar;
    constants.noise_scale = registry_->config_.volumetric_lighting.noise_scale;
    constants.noise_type = registry_->config_.volumetric_lighting.noise_type;

    vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(VolumetricLightingIntegrationPassConstants), &constants);
    vkCmdDispatch(cmd, dispatch_group_x, dispatch_group_y, dispatch_group_z);

  }

  void VolumetricLightingNoisePass::prepare() {
    fmt::print("Preparing {}\n", get_name());

    descriptor_layouts_.emplace_back(
        DescriptorLayoutBuilder()
            .add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(gpu_->getDevice()));

    dependencies_ = RenderPassDependencyBuilder()
                        .add_shader(ShaderStage::kCompute, "volumetric_light_noise.comp.spv")
                        .add_image_attachment(registry_->resources_.volumetric_noise_texture,
                                              ImageUsageType::kStore)
                        .set_push_constant_range(sizeof(VolumetricLightingIntegrationPassConstants),
                                                 VK_SHADER_STAGE_COMPUTE_BIT)
                        .build();

    create_pipeline_layout();

    pipeline_ = create_pipeline().build_compute_pipeline(gpu_->getDevice());
  }

  void VolumetricLightingNoisePass::destroy() {}

  void VolumetricLightingNoisePass ::execute(VkCommandBuffer cmd) {

    const auto global_frame = frame_->get_current_frame_number();
    if (global_frame > 5) {
      return;
    }
    const auto frame = frame_->get_current_frame_index();

    const auto volumetric_noise_texture = registry_->resources_.volumetric_noise_texture.image;

    if (descriptor_buffers_.at(frame) == nullptr) {
      descriptor_buffers_.at(frame)
          = resource_manager_->create_descriptor_buffer(descriptor_layouts_.at(0), 1, 0);
      auto image_info0
          = VkDescriptorImageInfo{post_process_sampler, volumetric_noise_texture->imageView,
                                  volumetric_noise_texture->getLayout()};
      descriptor_buffers_.at(frame)
          ->write_image(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, image_info0)
          .update();
    }

    bind_descriptor_buffers(cmd, {descriptor_buffers_.at(frame).get()});
    descriptor_buffers_.at(frame)->bind_descriptors(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                                    pipeline_layout_, 0);
    const auto dispatch_group_x
        = (uint32)ceil(volumetric_noise_texture->getExtent2D().width / 8.0f);
    const auto dispatch_group_y
        = (uint32)ceil(volumetric_noise_texture->getExtent2D().height / 8.0f);
    const auto dispatch_group_z = 128;

    begin_compute_pass(cmd);

    vkCmdDispatch(cmd, dispatch_group_x, dispatch_group_y, dispatch_group_z);
  }

}  // namespace gestalt::graphics