#pragma once
#include "Utils/CommandBuffer.hpp"
#include "Utils/Pipeline.hpp"
#include "Utils/ResourceBindings.hpp"
#include <RenderConfig.hpp>

#include "RenderPass.hpp"


namespace gestalt::graphics::fg {

  class SkyboxPass final : public RenderPass {
    ResourceComponent resources_;
    GraphicsPipeline graphics_pipeline_;
    std::function<RenderConfig::SkyboxParams()> push_constant_provider_;

  public:
    SkyboxPass(const std::shared_ptr<BufferInstance>& camera_buffer,
               const std::shared_ptr<BufferInstance>& directional_light,
               const std::shared_ptr<ImageInstance>& scene_lit,
               const std::shared_ptr<ImageInstance>& texEnvMap,
               const std::shared_ptr<ImageInstance>& scene_skybox,
                const std::shared_ptr<ImageInstance>& g_buffer_depth, 
        const VkSampler post_process_sampler,
        IGpu* gpu, const std::function<RenderConfig::SkyboxParams()>& push_constant_provider
    )
        : RenderPass("Skybox Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, camera_buffer, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                               VK_SHADER_STAGE_VERTEX_BIT
                                   | VK_SHADER_STAGE_FRAGMENT_BIT)
                  .add_binding(1, 1, directional_light, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                  .add_binding(2, 0, scene_lit, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                  .add_binding(2, 1, texEnvMap, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                  .add_push_constant(sizeof(RenderConfig::SkyboxParams),
                                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                  .add_attachment(scene_skybox)
                  .add_attachment(g_buffer_depth))),
          graphics_pipeline_(
              gpu, get_name(), resources_.get_image_bindings(), resources_.get_buffer_bindings(),
              resources_.get_image_array_bindings(), resources_.get_push_constant_range(),
              "skybox.vert.spv", "skybox.frag.spv",
              GraphicsPipelineBuilder()
                  .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                  .set_polygon_mode(VK_POLYGON_MODE_FILL)
                  .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                  .set_multisampling_none()
                  .disable_blending()
                  .enable_depthtest(false, VK_COMPARE_OP_LESS_OR_EQUAL)
                  .set_color_attachment_formats(resources_.get_color_attachments())
                  .set_depth_format(resources_.get_depth_attachment())
                  .build_pipeline_info()), push_constant_provider_(push_constant_provider) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers() override {
      return graphics_pipeline_.get_descriptor_buffers();
    }

    void execute(const CommandBuffer cmd) override {
      auto clear_color = VkClearColorValue{0.0f, 0.0f, 0.0f, 0.0f};
      graphics_pipeline_.begin_render_pass(cmd, resources_.get_color_attachments(),
                                           resources_.get_depth_attachment(), {clear_color});
      graphics_pipeline_.bind(cmd);

      auto push_constant = push_constant_provider_();
      cmd.push_constants(graphics_pipeline_.get_pipeline_layout(),
                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(RenderConfig::SkyboxParams), &push_constant);

      cmd.draw(36, 1, 0, 0);
      cmd.end_rendering();
    }
  };
}
