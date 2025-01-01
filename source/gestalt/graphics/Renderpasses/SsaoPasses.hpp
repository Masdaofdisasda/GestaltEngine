#pragma once
#include "Utils/CommandBuffer.hpp"
#include "Utils/Pipeline.hpp"
#include "Utils/ResourceBindings.hpp"
#include <RenderConfig.hpp>

#include "RenderPass.hpp"


namespace gestalt::graphics {

  class SsaoPass final : public RenderPass {
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<RenderConfig::SsaoParams()>
        push_constant_provider_; 
  public:
    SsaoPass(const std::shared_ptr<BufferInstance>& camera_buffer,
             const std::shared_ptr<ImageInstance>& g_buffer_depth,
             const std::shared_ptr<ImageInstance>& g_buffer_2,
             const std::shared_ptr<ImageInstance>& rotation_texture,
             const std::shared_ptr<ImageInstance>& ambient_occlusion,
             const VkSampler post_process_sampler, IGpu* gpu,
             const std::function<RenderConfig::SsaoParams()>& push_constant_provider)
        : RenderPass("Ssao Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, camera_buffer, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 0, g_buffer_depth, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 1, g_buffer_2, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 2, rotation_texture, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 3, ambient_occlusion, nullptr, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_push_constant(sizeof(RenderConfig::SsaoParams),
                                     VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_image_array_bindings(),
                            resources_.get_push_constant_range(),
                            "ssao_filter.comp.spv"),
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

      const auto push_constants = push_constant_provider_();
      cmd.push_constants(compute_pipeline_.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(RenderConfig::SsaoParams), &push_constants);
      const auto [width, height, _] = resources_.get_image_binding(1, 3).resource->get_extent();
      cmd.dispatch(static_cast<uint32>(ceil(width / 8)), static_cast<uint32>(ceil(height / 8)), 1);
    }
  };

}
