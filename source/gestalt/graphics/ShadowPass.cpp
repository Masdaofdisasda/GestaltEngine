#pragma once
#include "RenderPass.h"

#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"


namespace gestalt {
  namespace graphics {
    using namespace foundation;

    void DirectionalDepthPass::prepare() {
      fmt::print("Preparing {}\n", get_name());

      const auto& per_frame_buffers = repository_->get_buffer<PerFrameDataBuffers>();
      const auto& light_data = repository_->get_buffer<LightBuffers>();
      const auto& mesh_buffers = repository_->get_buffer<MeshBuffers>();
      descriptor_layouts_.push_back(per_frame_buffers.descriptor_layout);
      descriptor_layouts_.push_back(light_data.descriptor_layout);
      descriptor_layouts_.push_back(mesh_buffers.descriptor_layout);

      dependencies_ = RenderPassDependencyBuilder()
                          .add_shader(ShaderStage::kVertex, "shadow_geometry.vert.spv")
                          .add_shader(ShaderStage::kFragment, "shadow_depth.frag.spv")
                          .add_image_attachment(registry_->attachments_.shadow_map,
                                                ImageUsageType::kWrite, 0, ImageClearOperation::kClear)
                .set_push_constant_range(sizeof(GpuDrawPushConstants), VK_SHADER_STAGE_VERTEX_BIT)
                          .build();

      create_pipeline_layout();

      pipeline_ = create_pipeline()
                      .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                      .set_polygon_mode(VK_POLYGON_MODE_FILL)
                      .set_cull_mode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                      .set_multisampling_none()
                      .disable_blending()
                      .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
                      .build_pipeline(gpu_.device);
    }

    void DirectionalDepthPass::destroy() {  }

    void DirectionalDepthPass::execute(VkCommandBuffer cmd) {

      begin_renderpass(cmd);

      const auto frame = gpu_frame.get_current_frame();
      const auto& per_frame_buffers = repository_->get_buffer<PerFrameDataBuffers>();
      const auto& light_data = repository_->get_buffer<LightBuffers>();
      const auto& mesh_buffers = repository_->get_buffer<MeshBuffers>();

      vkCmdBindIndexBuffer(cmd, mesh_buffers.index_buffer.buffer, 0,
                           VK_INDEX_TYPE_UINT32);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

      VkDescriptorSet descriptorSets[] = {per_frame_buffers.descriptor_sets[frame],
                                          light_data.descriptor_set, mesh_buffers.descriptor_set};

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 3,
                              descriptorSets, 0, nullptr);
      vkCmdSetViewport(cmd, 0, 1, &viewport_);
      vkCmdSetScissor(cmd, 0, 1, &scissor_);

      for (auto& r : repository_->main_draw_context_.opaque_surfaces) {
        GpuDrawPushConstants push_constants;
        push_constants.worldMatrix = r.transform;
        push_constants.material_id = r.material;
        vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(GpuDrawPushConstants), &push_constants);
        vkCmdDrawIndexed(cmd, r.index_count, 1, r.first_index, 0, 0);
      }
      for (auto& r : repository_->main_draw_context_.transparent_surfaces) {
        GpuDrawPushConstants push_constants;
        push_constants.worldMatrix = r.transform;
        push_constants.material_id = r.material;
        vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(GpuDrawPushConstants), &push_constants);
        vkCmdDrawIndexed(cmd, r.index_count, 1, r.first_index, 0, 0);
      }

      vkCmdEndRendering(cmd);
    }
  }  // namespace graphics
}  // namespace gestalt