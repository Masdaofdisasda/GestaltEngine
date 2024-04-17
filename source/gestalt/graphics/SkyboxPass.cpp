#include "SkyboxPass.h"

#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"

namespace gestalt {
  namespace graphics {

    using namespace foundation;
    void SkyboxPass::prepare() {
      fmt::print("Preparing {}\n", get_name());

      dependencies_
          = RenderPassDependencyBuilder()
                .add_shader(ShaderStage::kVertex, "skybox.vert.spv")
                .add_shader(ShaderStage::kFragment, "skybox.frag.spv")
                .add_image_attachment(registry_->attachments_.scene_color, ImageUsageType::kWrite)
                .add_image_attachment(registry_->attachments_.scene_depth, ImageUsageType::kWrite)
                .set_push_constant_range(sizeof(RenderConfig::SkyboxParams),
                                         VK_SHADER_STAGE_FRAGMENT_BIT)
                .build();

      const auto& per_frame_buffers = repository_->get_buffer<PerFrameDataBuffers>();
      auto& light_data = repository_->get_buffer<LightBuffers>();
      descriptor_layouts_.push_back(per_frame_buffers.descriptor_layout);
      descriptor_layouts_.push_back(light_data.descriptor_layout);

      create_pipeline_layout();

      pipeline_ = create_pipeline()
                      .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                      .set_polygon_mode(VK_POLYGON_MODE_FILL)
                      .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                      .set_multisampling_none()
                      .disable_blending()
                      .enable_depthtest(false, VK_COMPARE_OP_LESS_OR_EQUAL)
                      .build_pipeline(gpu_.device);
    }

    void SkyboxPass::execute(const VkCommandBuffer cmd) {
      begin_renderpass(cmd);

      const char frameIndex = gpu_.get_current_frame();
      const auto& per_frame_buffers = repository_->get_buffer<PerFrameDataBuffers>();
      auto& light_data = repository_->get_buffer<LightBuffers>();

      VkDescriptorSet descriptorSets[]
          = {per_frame_buffers.descriptor_sets[frameIndex], light_data.descriptor_set};

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 2,
                              descriptorSets, 0, nullptr);
      vkCmdSetViewport(cmd, 0, 1, &viewport_);
      vkCmdSetScissor(cmd, 0, 1, &scissor_);

      vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(RenderConfig::SkyboxParams), &registry_->config_.skybox);
      vkCmdDraw(cmd, 36, 1, 0, 0);  // 36 vertices for the cube

      vkCmdEndRendering(cmd);
    }

    void SkyboxPass::cleanup() {
      vkDestroyPipelineLayout(gpu_.device, pipeline_layout_, nullptr);
      vkDestroyPipeline(gpu_.device, pipeline_, nullptr);
    }

    void InfiniteGridPass::prepare() {
      fmt::print("Preparing {}\n", get_name());

      dependencies_
          = RenderPassDependencyBuilder()
                .add_shader(ShaderStage::kVertex, "infinite_grid.vert.spv")
                .add_shader(ShaderStage::kFragment, "infinite_grid.frag.spv")
                .add_image_attachment(registry_->attachments_.scene_color, ImageUsageType::kWrite)
                .add_image_attachment(registry_->attachments_.scene_depth, ImageUsageType::kWrite)
                .build();

      const auto& per_frame_buffers = repository_->get_buffer<PerFrameDataBuffers>();
      descriptor_layouts_.push_back(per_frame_buffers.descriptor_layout);

      VkPipelineLayoutCreateInfo pipeline_layout_create_info{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
          .pNext = nullptr,
          .setLayoutCount = static_cast<uint32_t>(descriptor_layouts_.size()),
          .pSetLayouts = descriptor_layouts_.data(),
          .pushConstantRangeCount = 1,
          .pPushConstantRanges = &push_constant_range,
      };

      VK_CHECK(vkCreatePipelineLayout(gpu_.device, &pipeline_layout_create_info, nullptr,
                                      &pipeline_layout_));

      pipeline_ = create_pipeline()
                      .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                      .set_polygon_mode(VK_POLYGON_MODE_FILL)
                      .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                      .set_multisampling_none()
                      .disable_blending()
                      .enable_depthtest(false, VK_COMPARE_OP_LESS_OR_EQUAL)
                      .build_pipeline(gpu_.device);
    }

    void InfiniteGridPass::execute(const VkCommandBuffer cmd) {
      begin_renderpass(cmd);

      const char frameIndex = gpu_.get_current_frame();
      const auto& per_frame_buffers = repository_->get_buffer<PerFrameDataBuffers>();

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

      VkDescriptorSet descriptorSets[] = {per_frame_buffers.descriptor_sets[frameIndex]};

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                              descriptorSets, 0, nullptr);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
      vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(RenderConfig::GridParams), &registry_->config_.grid);
      vkCmdSetViewport(cmd, 0, 1, &viewport_);
      vkCmdSetScissor(cmd, 0, 1, &scissor_);
      vkCmdDraw(cmd, 6, 1, 0, 0);  // 6 vertices for the grid

      vkCmdEndRendering(cmd);
    }

    void InfiniteGridPass::cleanup() {
      vkDestroyPipelineLayout(gpu_.device, pipeline_layout_, nullptr);
      vkDestroyPipeline(gpu_.device, pipeline_, nullptr);
    }
  }  // namespace graphics
}  // namespace gestalt