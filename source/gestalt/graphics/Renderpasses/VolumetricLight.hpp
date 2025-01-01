#pragma once
#include <PerFrameData.hpp>
#include <RenderConfig.hpp>

#include "RenderPass.hpp"
#include "Utils/CommandBuffer.hpp"
#include "Utils/Pipeline.hpp"
#include "Utils/ResourceBindings.hpp"

    namespace gestalt::graphics {

  class VolumetricLightingInjectionPass final : public RenderPass {
    struct alignas(16) VolumetricLightingInjectionPassConstants {
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

      float32 volumetric_noise_position_multiplier{1.f};
      float32 volumetric_noise_speed_multiplier{1.f};
      float32 height_fog_density{0.01f};
      float32 height_fog_falloff{1.f};

      glm::vec3 box_position{0.f};
      float32 box_fog_density{0.1f};

      glm::vec3 box_size{10.f};
      float32 scattering_factor{0.5f};
    };
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<RenderConfig::VolumetricLightingParams()> push_constant_provider_;
    std::function<uint32()> frame_provider_;
    std::function<PerFrameData()> camera_provider_;

  public:
    VolumetricLightingInjectionPass(
        const std::shared_ptr<ImageInstance>& blue_noise,
        const std::shared_ptr<ImageInstance>& noise_texture,
        const std::shared_ptr<ImageInstance>& froxel_data, const VkSampler post_process_sampler,
        const std::function<RenderConfig::VolumetricLightingParams()>& push_constant_provider,
        const std::function<uint32()>& frame_provider,
        const std::function<PerFrameData()>& camera_provider, IGpu* gpu)
        : RenderPass("Volumetric Lighting Injection Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, blue_noise, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(0, 1, noise_texture, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(0, 2, froxel_data, nullptr, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_push_constant(sizeof(VolumetricLightingInjectionPassConstants),
                                     VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_image_array_bindings(),
                            resources_.get_push_constant_range(),
                            "volumetric_light_injection.comp.spv"),
          push_constant_provider_(push_constant_provider),
          frame_provider_(frame_provider),
          camera_provider_(camera_provider) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers() override {
      return compute_pipeline_.get_descriptor_buffers();
    }

    void execute(const CommandBuffer cmd) override {
      compute_pipeline_.bind(cmd);

      const auto params = push_constant_provider_();
      const auto perFrameData = camera_provider_();
      VolumetricLightingInjectionPassConstants push_constants;
      push_constants.current_frame = frame_provider_();
      push_constants.froxel_inverse_view_projection = perFrameData.inv_viewProj;
      push_constants.density_modifier = params.density_modifier;
      push_constants.noise_scale = params.noise_scale;
      push_constants.noise_type = params.noise_type;
      push_constants.volumetric_noise_position_multiplier
          = params.volumetric_noise_position_multiplier;
      push_constants.froxel_near = perFrameData.znear;
      push_constants.froxel_far = perFrameData.zfar;
      push_constants.height_fog_density = params.height_fog_density;
      push_constants.height_fog_falloff = params.height_fog_falloff;
      push_constants.box_position = params.box_position;
      push_constants.box_fog_density = params.box_fog_density;
      push_constants.box_size = params.box_size;
      push_constants.scattering_factor = params.scattering_factor;

      cmd.push_constants(compute_pipeline_.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(VolumetricLightingInjectionPassConstants), &push_constants);
      const auto [width, height, _] = resources_.get_image_binding(0, 2).resource->get_extent();
      cmd.dispatch(static_cast<uint32>(ceil(width / 8)), static_cast<uint32>(ceil(height / 8)),
                   128);
    }
  };

  class VolumetricLightingScatteringPass final : public RenderPass {
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
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<RenderConfig::VolumetricLightingParams()> push_constant_provider_;
    std::function<uint32()> frame_provider_;
    std::function<PerFrameData()> camera_provider_;
    std::function<uint32()> point_light_count_provider_;

  public:
    VolumetricLightingScatteringPass(
        const std::shared_ptr<BufferInstance>& camera_buffer,
        const std::shared_ptr<BufferInstance>& light_matrices,
        const std::shared_ptr<BufferInstance>& directional_lights,
        const std::shared_ptr<BufferInstance>& points_lights,
        const std::shared_ptr<ImageInstance>& blue_noise,
        const std::shared_ptr<ImageInstance>& froxel_data,
        const std::shared_ptr<ImageInstance>& shadow_map,
        const std::shared_ptr<ImageInstance>& light_scattering,
        const VkSampler post_process_sampler,
        const std::function<RenderConfig::VolumetricLightingParams()>& push_constant_provider,
        const std::function<uint32()>& frame_provider,
        const std::function<PerFrameData()>& camera_provider,
        const std::function<uint32()>& point_light_count_provider, IGpu* gpu)
        : RenderPass("Volumetric Lighting Scattering Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, camera_buffer, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)

                  .add_binding(1, 0, blue_noise, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 1, froxel_data, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 2, shadow_map, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 3, light_scattering, nullptr, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)

                  .add_binding(2, 0, light_matrices, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(2, 1, directional_lights, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(2, 2, points_lights, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_push_constant(sizeof(VolumetricLightingScatteringPassConstants),
                                     VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_image_array_bindings(),
                            resources_.get_push_constant_range(),
                            "volumetric_light_scattering.comp.spv"),
          push_constant_provider_(push_constant_provider),
          frame_provider_(frame_provider),
          camera_provider_(camera_provider),
          point_light_count_provider_(point_light_count_provider) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers() override {
      return compute_pipeline_.get_descriptor_buffers();
    }

    void execute(const CommandBuffer cmd) override {
      compute_pipeline_.bind(cmd);

      const auto params = push_constant_provider_();
      const auto perFrameData = camera_provider_();
      VolumetricLightingScatteringPassConstants push_constants;
      push_constants.current_frame = frame_provider_();
      push_constants.froxel_inverse_view_projection = perFrameData.inv_viewProj;
      push_constants.froxel_near = perFrameData.znear;
      push_constants.froxel_far = perFrameData.zfar;
      push_constants.scattering_factor = params.scattering_factor;
      push_constants.phase_anisotropy = params.phase_anisotropy;
      push_constants.phase_type = params.phase_type;
      push_constants.density_modifier = params.density_modifier;
      push_constants.noise_scale = params.noise_scale;
      push_constants.noise_type = params.noise_type;
      push_constants.num_point_lights = point_light_count_provider_();

      cmd.push_constants(compute_pipeline_.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(VolumetricLightingScatteringPassConstants), &push_constants);
      const auto [width, height, _] = resources_.get_image_binding(1, 3).resource->get_extent();
      cmd.dispatch(static_cast<uint32>(ceil(width / 8)), static_cast<uint32>(ceil(height / 8)),
                   128);
    }
  };

  class VolumetricLightingSpatialFilterPass final : public RenderPass {
    struct VolumetricLightingSpatialFilterPassConstants {
      glm::uvec3 froxel_dimensions{128, 128, 128};
      int32 use_spatial_filtering{1};
    };
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<RenderConfig::VolumetricLightingParams()> push_constant_provider_;

  public:
    VolumetricLightingSpatialFilterPass(
        const std::shared_ptr<ImageInstance>& light_scattering,
        const std::shared_ptr<ImageInstance>& light_scattering_filtered,
        const VkSampler post_process_sampler,
        const std::function<RenderConfig::VolumetricLightingParams()>& push_constant_provider,
        IGpu* gpu)
        : RenderPass("Volumetric Lighting Spatial Filter Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, light_scattering, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(0, 1, light_scattering_filtered, nullptr, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_push_constant(sizeof(VolumetricLightingSpatialFilterPassConstants),
                                     VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_image_array_bindings(),
                            resources_.get_push_constant_range(),
                            "volumetric_light_spatial_filter.comp.spv"),
          push_constant_provider_(push_constant_provider) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers() override {
      return compute_pipeline_.get_descriptor_buffers();
    }

    void execute(const CommandBuffer cmd) override {
      compute_pipeline_.bind(cmd);

      const auto params = push_constant_provider_();
      VolumetricLightingSpatialFilterPassConstants push_constants;
      push_constants.use_spatial_filtering = params.enable_spatial_filter;

      cmd.push_constants(compute_pipeline_.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(VolumetricLightingSpatialFilterPassConstants), &push_constants);
      const auto [width, height, _] = resources_.get_image_binding(0, 1).resource->get_extent();
      cmd.dispatch(static_cast<uint32>(ceil(width / 8)), static_cast<uint32>(ceil(height / 8)),
                   128);
    }
  };

  class VolumetricLightingIntegrationPass final : public RenderPass {
    struct alignas(16) VolumetricLightingIntegrationPassConstants {
      glm::uvec3 froxel_dimensions{128, 128, 128};
      int32 current_frame{0};

      float32 noise_scale{1.f};
      uint32 noise_type{0};
      float32 froxel_near{1.f};
      float32 froxel_far{100.f};
    };
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<RenderConfig::VolumetricLightingParams()> push_constant_provider_;
    std::function<uint32()> frame_provider_;
    std::function<PerFrameData()> camera_provider_;

  public:
    VolumetricLightingIntegrationPass(
        const std::shared_ptr<ImageInstance>& light_scattering_filtered,
        const std::shared_ptr<ImageInstance>& light_integrated,
        const VkSampler post_process_sampler,
        const std::function<RenderConfig::VolumetricLightingParams()>& push_constant_provider,
        const std::function<uint32()>& frame_provider,
        const std::function<PerFrameData()>& camera_provider, IGpu* gpu)
        : RenderPass("Volumetric Lighting Integration Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, light_scattering_filtered, post_process_sampler,
                               ResourceUsage::READ, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(0, 1, light_integrated, nullptr, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_push_constant(sizeof(VolumetricLightingIntegrationPassConstants),
                                     VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_image_array_bindings(),
                            resources_.get_push_constant_range(),
                            "volumetric_light_integration.comp.spv"),
          push_constant_provider_(push_constant_provider),
          frame_provider_(frame_provider),
          camera_provider_(camera_provider) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers() override {
      return compute_pipeline_.get_descriptor_buffers();
    }

    void execute(const CommandBuffer cmd) override {
      compute_pipeline_.bind(cmd);

      const auto params = push_constant_provider_();
      const auto perFrameData = camera_provider_();
      VolumetricLightingIntegrationPassConstants push_constants;
      push_constants.current_frame = frame_provider_();
      push_constants.froxel_near = perFrameData.znear;
      push_constants.froxel_far = perFrameData.zfar;
      push_constants.noise_scale = params.noise_scale;
      push_constants.noise_type = params.noise_type;

      cmd.push_constants(compute_pipeline_.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(VolumetricLightingIntegrationPassConstants), &push_constants);
      const auto [width, height, _] = resources_.get_image_binding(0, 1).resource->get_extent();
      cmd.dispatch(static_cast<uint32>(ceil(width / 8)), static_cast<uint32>(ceil(height / 8)), 1);
    }
  };

  class VolumetricLightingNoisePass final : public RenderPass {
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<RenderConfig::VolumetricLightingParams()> push_constant_provider_;
    std::function<uint32()> frame_provider_;
    std::function<PerFrameData()> camera_provider_;

  public:
    VolumetricLightingNoisePass(const std::shared_ptr<ImageInstance>& volumetric_noise_texture,
                                IGpu* gpu)
        : RenderPass("Volumetric Lighting Noise Pass"),
          resources_(std::move(ResourceComponentBindings().add_binding(
              0, 0, volumetric_noise_texture, nullptr, ResourceUsage::WRITE,
              VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_image_array_bindings(),
                            resources_.get_push_constant_range(),
                            "volumetric_light_noise.comp.spv") {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers() override {
      return compute_pipeline_.get_descriptor_buffers();
    }

    void execute(const CommandBuffer cmd) override {
      compute_pipeline_.bind(cmd);
      const auto [width, height, _] = resources_.get_image_binding(0, 0).resource->get_extent();
      cmd.dispatch(static_cast<uint32>(ceil(width / 8)), static_cast<uint32>(ceil(height / 8)), 64);
    }
  };
}