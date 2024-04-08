#pragma once
#include "ShadowPass.h"

#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"


namespace gestalt {
  namespace graphics {

    using namespace foundation;

    void DirectionalDepthPass::prepare() {
      fmt::print("Preparing directional depth pass\n");

      const auto& per_frame_buffers = repository_->get_buffer<PerFrameDataBuffers>();
      descriptor_layouts_.push_back(per_frame_buffers.descriptor_layout);
      descriptor_layouts_.emplace_back(
          DescriptorLayoutBuilder()
              .add_binding(17, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, false,
                           kLimits.max_directional_lights + kLimits.max_point_lights)
              .build(gpu_.device));
      descriptor_layouts_.push_back(resource_manager_->scene_geometry_.vertex_layout);

      VkShaderModule meshFragShader;
      vkutil::load_shader_module(fragment_shader_source_.c_str(), gpu_.device, &meshFragShader);

      VkShaderModule meshVertexShader;
      vkutil::load_shader_module(vertex_shader_source_.c_str(), gpu_.device, &meshVertexShader);

      VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
      mesh_layout_info.setLayoutCount = descriptor_layouts_.size();
      mesh_layout_info.pSetLayouts = descriptor_layouts_.data();
      mesh_layout_info.pPushConstantRanges = &push_constant_range_;
      mesh_layout_info.pushConstantRangeCount = 1;

      VK_CHECK(vkCreatePipelineLayout(gpu_.device, &mesh_layout_info, nullptr, &pipeline_layout_));

      const auto depth_image = registry_->get_resource<TextureHandle>("directional_depth");

      pipeline_ = PipelineBuilder()
                      .set_shaders(meshVertexShader, meshFragShader)
                      .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                      .set_polygon_mode(VK_POLYGON_MODE_FILL)
                      .set_cull_mode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                      .set_multisampling_none()
                      .disable_blending()
                      .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
                      .enable_dynamic_depth_bias()
                      .set_depth_format(depth_image->imageFormat)
                      .set_pipeline_layout(pipeline_layout_)
                      .build_pipeline(gpu_.device);

      vkDestroyShaderModule(gpu_.device, meshFragShader, nullptr);
      vkDestroyShaderModule(gpu_.device, meshVertexShader, nullptr);
    }

    void DirectionalDepthPass::cleanup() { vkDestroyPipeline(gpu_.device, pipeline_, nullptr); }

    void DirectionalDepthPass::execute(VkCommandBuffer cmd) {
      descriptor_set_
          = resource_manager_->descriptor_pool->allocate(gpu_.device, descriptor_layouts_.at(1));

      const auto depth_image = registry_->get_resource<TextureHandle>("directional_depth");

      VkClearValue depth_clear = {.depthStencil = {1.f, 0}};
      VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(
          depth_image->imageView, &depth_clear, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

      VkRenderingInfo renderInfo
          = vkinit::rendering_info(depth_image->getExtent2D(), nullptr, &depthAttachment);

      vkCmdBeginRendering(cmd, &renderInfo);

      const char frameIndex = gpu_.get_current_frame();
      const auto& per_frame_buffers = repository_->get_buffer<PerFrameDataBuffers>();

      VkDescriptorBufferInfo buffer_info;
      buffer_info.buffer = per_frame_buffers.uniform_buffers[frameIndex].buffer;
      buffer_info.offset = 0;
      buffer_info.range = sizeof(PerFrameData);

      VkWriteDescriptorSet descriptor_write = {};
      descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptor_write.dstBinding = 0;
      descriptor_write.dstArrayElement = 0;
      descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      descriptor_write.descriptorCount = 1;
      descriptor_write.pBufferInfo = &buffer_info;

      vkCmdBindIndexBuffer(cmd, resource_manager_->scene_geometry_.indexBuffer.buffer, 0,
                           VK_INDEX_TYPE_UINT32);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

      auto& light_data = repository_->get_buffer<LightData>();
      writer.clear();

      std::vector<VkDescriptorBufferInfo> lightViewProjBufferInfos; //TODO : move this
      for (int i = 0; i < kLimits.max_directional_lights + kLimits.max_point_lights; ++i) {
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = light_data.view_proj_matrices.buffer;
        bufferInfo.offset = 64 * i;
        bufferInfo.range = 64;
        lightViewProjBufferInfos.push_back(bufferInfo);
      }
      writer.write_buffer_array(17, lightViewProjBufferInfos, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);

      VkDescriptorSet descriptorSets[]
          = {descriptor_set_, resource_manager_->scene_geometry_.vertex_set};

      writer.update_set(gpu_.device, descriptor_set_);
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 1, 2,
                              descriptorSets, 0, nullptr);

      gpu_.vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                                     &descriptor_write);

      // Depth bias (and slope) are used to avoid shadowing artifacts
      // Constant depth bias factor (always applied)
      float depthBiasConstant = registry_->config_.shadow.shadow_bias;
      // Slope depth bias factor, applied depending on polygon's slope
      float depthBiasSlope = 1.75f;
      vkCmdSetDepthBias(cmd, depthBiasConstant, 0.0f, depthBiasSlope);
      viewport_.width = static_cast<float>(depth_image->getExtent2D().width);
      viewport_.height = static_cast<float>(depth_image->getExtent2D().height);
      scissor_.extent = depth_image->getExtent2D();
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