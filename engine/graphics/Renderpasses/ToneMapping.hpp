#pragma once
#include "Utils/CommandBuffer.hpp"
#include "Utils/Pipeline.hpp"
#include "Utils/ResourceBindings.hpp"
#include <RenderConfig.hpp>

#include "RenderPass.hpp"


namespace gestalt::graphics {
  class ToneMapPass final : public RenderPass {
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<RenderConfig::HdrParams()> push_constant_provider_;

  public:
    ToneMapPass(const std::shared_ptr<ImageInstance>& scene_linear,
                const std::shared_ptr<ImageInstance>& scene_tone_mapped,
                const std::shared_ptr<ImageInstance>& luminance_average,
                const VkSampler post_process_sampler, IGpu& gpu,
                const std::function<RenderConfig::HdrParams()>& push_constant_provider
        )
        : RenderPass("Tone Map Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, scene_linear, nullptr, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(0, 2, luminance_average, nullptr, ResourceUsage::READ, 
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(0, 3, scene_tone_mapped, nullptr, ResourceUsage::WRITE, 
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_push_constant(sizeof(RenderConfig::HdrParams), VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(&gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_image_array_bindings(),
                            resources_.get_push_constant_range(),
                            "tone_mapping.comp.spv", resources_.get_tlas_bindings()), push_constant_provider_(push_constant_provider) {}

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

      const auto [width, height, _] = resources_.get_image_binding(0, 3).resource->get_extent();
      const uint32 groupX = (width + 16 - 1) / 16;
      const uint32 groupY = (height + 16 - 1) / 16;
      cmd.dispatch(groupX, groupY, 1);
    }
  };

  class LuminanceHistogramPass final : public RenderPass {
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<RenderConfig::LuminanceParams()> push_constant_provider_;

  public:
    LuminanceHistogramPass(
        const std::shared_ptr<ImageInstance>& scene,
        const std::shared_ptr<BufferInstance>& luminance_histogram,
        const VkSampler post_process_sampler, IGpu& gpu,
        const std::function<RenderConfig::LuminanceParams()>& push_constant_provider)
        : RenderPass("Luminance Histogram Pass"),
          resources_(
              std::move(ResourceComponentBindings()
                            .add_binding(0, 0, luminance_histogram, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                         VK_SHADER_STAGE_COMPUTE_BIT)
                            .add_binding(0, 1, scene, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                         VK_SHADER_STAGE_COMPUTE_BIT)
                            .add_push_constant(sizeof(RenderConfig::LuminanceParams),
                                               VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(&gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_image_array_bindings(),
                            resources_.get_push_constant_range(), "luminance_histogram.comp.spv",
                            resources_.get_tlas_bindings()),
          push_constant_provider_(push_constant_provider) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers() override {
      return compute_pipeline_.get_descriptor_buffers();
    }

    void execute(CommandBuffer cmd) override {
      compute_pipeline_.bind(cmd);

      auto params = push_constant_provider_();
      cmd.push_constants(compute_pipeline_.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(RenderConfig::LuminanceParams), &params);

      const auto [width, height, _] = resources_.get_image_binding(0, 1).resource->get_extent();
      params.num_pixels = width * height;
      params.width = width;
      params.height = height;
      const uint32 groupX = (width + 16 - 1) / 16;
      const uint32 groupY = (height + 16 - 1) / 16;
      cmd.dispatch(groupX, groupY, 1);
    }
  };

  class LuminanceAveragePass final : public RenderPass {
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<RenderConfig::LuminanceParams()> push_constant_provider_;

  public:
    LuminanceAveragePass(
        const std::shared_ptr<ImageInstance>& lum_tex,
        const std::shared_ptr<BufferInstance>& luminance_histogram, IGpu& gpu,
        const std::function<RenderConfig::LuminanceParams()>& push_constant_provider)
        : RenderPass("Luminance Average Pass"),
          resources_(std::move(ResourceComponentBindings()
                                   .add_binding(0, 0, luminance_histogram, ResourceUsage::READ,
                                                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                VK_SHADER_STAGE_COMPUTE_BIT)
                                   .add_binding(0, 1, lum_tex, nullptr, ResourceUsage::WRITE,
                                                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                VK_SHADER_STAGE_COMPUTE_BIT)
                                   .add_push_constant(sizeof(RenderConfig::LuminanceParams),
                                                      VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(&gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_image_array_bindings(),
                            resources_.get_push_constant_range(), "luminance_average.comp.spv"),
          push_constant_provider_(push_constant_provider) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers() override {
      return compute_pipeline_.get_descriptor_buffers();
    }

    void execute(CommandBuffer cmd) override {
      compute_pipeline_.bind(cmd);

      const auto params = push_constant_provider_();
      cmd.push_constants(compute_pipeline_.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(RenderConfig::LuminanceParams), &params);

      cmd.dispatch(1, 1, 1);
    }
  };
}
