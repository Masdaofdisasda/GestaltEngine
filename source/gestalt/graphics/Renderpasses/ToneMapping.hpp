#pragma once
#include "Utils/CommandBuffer.hpp"
#include "Utils/Pipeline.hpp"
#include "Utils/ResourceBindings.hpp"
#include <RenderConfig.hpp>

#include "RenderPass.hpp"


namespace gestalt::graphics::fg {
  class ToneMapPass final : public RenderPass {
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<RenderConfig::HdrParams()> push_constant_provider_;

  public:
    ToneMapPass(const std::shared_ptr<ImageInstance>& scene_final,
                const std::shared_ptr<ImageInstance>& scene_lit,
                const std::shared_ptr<ImageInstance>& scene_skybox,
                const VkSampler post_process_sampler, IGpu* gpu,
                const std::function<RenderConfig::HdrParams()>& push_constant_provider
        )
        : RenderPass("Tone Map Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding( 0, 0,scene_lit, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding( 0, 1,scene_skybox, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(0, 3, scene_final, nullptr, ResourceUsage::WRITE, 
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_push_constant(sizeof(RenderConfig::HdrParams), VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_image_array_bindings(),
                            resources_.get_push_constant_range(),
                            "tone_mapping.comp.spv"), push_constant_provider_(push_constant_provider) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers() override {
      return compute_pipeline_.get_descriptor_buffers();
    }

    void execute(CommandBuffer cmd) override {
      compute_pipeline_.bind(cmd);

      const auto params = push_constant_provider_();
      cmd.push_constants(compute_pipeline_.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(RenderConfig::HdrParams), &params);

      const auto [width, height, _] = resources_.get_image_binding(0, 1).resource->get_extent();
      cmd.dispatch(static_cast<uint32>(ceil(width / 8)), static_cast<uint32>(ceil(height / 8)), 1);
    }
  };
}
