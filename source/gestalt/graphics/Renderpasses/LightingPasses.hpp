#pragma once
#include "Utils/CommandBuffer.hpp"
#include "Utils/Pipeline.hpp"
#include "Utils/ResourceBindings.hpp"
#include <RenderConfig.hpp>
#include <PerFrameData.hpp>

#include "RenderPass.hpp"


namespace gestalt::graphics {

  class LightingPass final : public RenderPass {
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    const std::function<RenderConfig::LightingParams()> push_constant_provider_;
    std::function<PerFrameData()> camera_provider_;
    std::function<uint32()> dir_light_count_provider_;
    std::function<uint32()> point_light_count_provider_;
    std::function<uint32()> spot_light_count_provider_;

  public:
    LightingPass(const std::shared_ptr<BufferInstance>& camera_buffer,
                 const std::shared_ptr<ImageInstance>& tex_env_map,
                 const std::shared_ptr<ImageInstance>& tex_env_map_irradiance,
                 const std::shared_ptr<ImageInstance>& tex_bdrf_lut,
                 const std::shared_ptr<BufferInstance>& light_matrices,
                 const std::shared_ptr<BufferInstance>& directional_light,
                 const std::shared_ptr<BufferInstance>& point_light,
                 const std::shared_ptr<BufferInstance>& spot_light,
                 const std::shared_ptr<ImageInstance>& g_buffer_1,
                 const std::shared_ptr<ImageInstance>& g_buffer_2,
                 const std::shared_ptr<ImageInstance>& g_buffer_3,
                 const std::shared_ptr<ImageInstance>& g_buffer_depth,
                 const std::shared_ptr<ImageInstance>& shadow_map,
                 const std::shared_ptr<ImageInstance>& integrated_light_scattering,
                 const std::shared_ptr<ImageInstance>& ambient_occlusion,
                 const std::shared_ptr<ImageInstance>& scene_lit,
                 const std::shared_ptr<AccelerationStructureInstance>& tlas_instance,
                 const VkSampler post_process_sampler, const VkSampler interpolation_sampler,
                 const VkSampler cube_map_sampler, IGpu* gpu,
                 const std::function<RenderConfig::LightingParams()>& push_constant_provider,
                 const std::function<PerFrameData()>& camera_provider,
                 const std::function<uint32()>& dir_light_count_provider,
                 const std::function<uint32()>& point_light_count_provider,
                 const std::function<uint32()>& spot_light_count_provider)
        : RenderPass("Lighting Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, camera_buffer, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)

                  .add_binding(1, 0, tex_env_map, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 1, tex_env_map_irradiance, cube_map_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 2, tex_bdrf_lut, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)

                  .add_binding(2, 0, light_matrices, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(2, 1, directional_light, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(2, 2, point_light, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(2, 3, spot_light, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)

                  .add_binding(3, 0, g_buffer_1, nullptr, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(3, 1, g_buffer_2, nullptr, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(3, 2, g_buffer_3, nullptr, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(3, 3, g_buffer_depth, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(3, 4, shadow_map, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(3, 5, integrated_light_scattering, interpolation_sampler,
                               ResourceUsage::READ, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(3, 6, ambient_occlusion, nullptr, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(3, 7, scene_lit, nullptr, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(3, 8, tlas_instance, ResourceUsage::READ, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_push_constant(sizeof(RenderConfig::LightingParams),
                                     VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_image_array_bindings(),
                            resources_.get_push_constant_range(), "pbr_lighting.comp.spv",
                            resources_.get_tlas_bindings()),
          push_constant_provider_(push_constant_provider),
          camera_provider_(camera_provider),
          dir_light_count_provider_(dir_light_count_provider),
          point_light_count_provider_(point_light_count_provider),
          spot_light_count_provider_(spot_light_count_provider) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers() override {
      return compute_pipeline_.get_descriptor_buffers();
    }

    void execute(CommandBuffer cmd) override {
      compute_pipeline_.bind(cmd);

              auto params = push_constant_provider_();
      params.invViewProj = camera_provider_().inv_viewProj;
              params.num_dir_lights = dir_light_count_provider_();
      params.num_point_lights = point_light_count_provider_();
      params.num_spot_lights = spot_light_count_provider_();
              cmd.push_constants(compute_pipeline_.get_pipeline_layout(),
                                 VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                 sizeof(RenderConfig::LightingParams), &params);

      const auto [width, height, _] = resources_.get_image_binding(3, 7).resource->get_extent();
              const uint32 groupX = (width + 16 - 1) / 16;
              const uint32 groupY = (height + 16 - 1) / 16;
              cmd.dispatch(groupX, groupY, 1);
    }
  };
}
