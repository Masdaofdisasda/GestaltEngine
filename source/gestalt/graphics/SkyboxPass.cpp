#include "SkyboxPass.h"

#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"

namespace gestalt {
  namespace graphics {

    using namespace foundation;

    void SkyboxPass::prepare() {
      fmt::print("Preparing skybox pass\n");

      const auto& per_frame_buffers = repository_->get_buffer<PerFrameDataBuffers>();
      auto& light_data = repository_->get_buffer<LightData>();
      descriptor_layouts_.push_back(per_frame_buffers.descriptor_layout);
      descriptor_layouts_.push_back(light_data.light_layout);

      VkPipelineLayoutCreateInfo pipeline_layout_create_info{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
          .pNext = nullptr,
          .setLayoutCount = static_cast<uint32_t>(descriptor_layouts_.size()),
          .pSetLayouts = descriptor_layouts_.data(),
          .pushConstantRangeCount = 1,
          .pPushConstantRanges = &push_constant_range_,
      };

      VK_CHECK(vkCreatePipelineLayout(gpu_.device, &pipeline_layout_create_info, nullptr,
                                      &pipeline_layout_));

      VkShaderModule vertex_shader;
      vkutil::load_shader_module(vertex_shader_source_.c_str(), gpu_.device, &vertex_shader);
      VkShaderModule fragment_shader;
      vkutil::load_shader_module(fragment_shader_source_.c_str(), gpu_.device, &fragment_shader);

      const auto color_image = registry_->get_resource<TextureHandle>("scene_shaded");
      const auto depth_image = registry_->get_resource<TextureHandle>("gbuffer_depth");

      pipeline_ = PipelineBuilder()
                      .set_shaders(vertex_shader, fragment_shader)
                      .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                      .set_polygon_mode(VK_POLYGON_MODE_FILL)
                      .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                      .set_multisampling_none()
                      .disable_blending()
                      .enable_depthtest(false, VK_COMPARE_OP_LESS_OR_EQUAL)
                      .set_color_attachment_format(color_image->imageFormat)
                      .set_depth_format(depth_image->imageFormat)
                      .set_pipeline_layout(pipeline_layout_)
                      .build_pipeline(gpu_.device);

      // Clean up shader modules after pipeline creation
      vkDestroyShaderModule(gpu_.device, vertex_shader, nullptr);
      vkDestroyShaderModule(gpu_.device, fragment_shader, nullptr);
    }

    void SkyboxPass::execute(const VkCommandBuffer cmd) {
      const auto color_image = registry_->get_resource<TextureHandle>("scene_shaded");
      const auto depth_image = registry_->get_resource<TextureHandle>("gbuffer_depth");

      VkRenderingAttachmentInfo colorAttachment
          = vkinit::attachment_info(color_image->imageView, nullptr, color_image->currentLayout);

      VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(
          depth_image->imageView, nullptr, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

      VkRenderingInfo renderInfo
          = vkinit::rendering_info(color_image->getExtent2D(), &colorAttachment, &depthAttachment);

      vkCmdBeginRendering(cmd, &renderInfo);

      const char frameIndex = gpu_.get_current_frame();
      const auto& per_frame_buffers = repository_->get_buffer<PerFrameDataBuffers>();
      auto& light_data = repository_->get_buffer<LightData>();

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

      gpu_.vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                                     &descriptor_write);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 1, 1,
                              &light_data.light_set, 0, nullptr);

      viewport_.width = static_cast<float>(color_image->getExtent2D().width);
      viewport_.height = static_cast<float>(color_image->getExtent2D().height);

      vkCmdSetViewport(cmd, 0, 1, &viewport_);

      scissor_.extent.width = color_image->getExtent2D().width;
      scissor_.extent.height = color_image->getExtent2D().height;

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
      fmt::print("Preparing skybox pass\n");

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

      VkShaderModule vertex_shader;
      vkutil::load_shader_module(vertex_shader_source_.c_str(), gpu_.device, &vertex_shader);
      VkShaderModule fragment_shader;
      vkutil::load_shader_module(fragment_shader_source_.c_str(), gpu_.device, &fragment_shader);

      const auto color_image = registry_->get_resource<TextureHandle>("skybox_color");
      const auto depth_image = registry_->get_resource<TextureHandle>("skybox_depth");

      pipeline_ = PipelineBuilder()
                      .set_shaders(vertex_shader, fragment_shader)
                      .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                      .set_polygon_mode(VK_POLYGON_MODE_FILL)
                      .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                      .set_multisampling_none()
                      .disable_blending()
                      .enable_depthtest(false, VK_COMPARE_OP_LESS_OR_EQUAL)
                      .set_color_attachment_format(color_image->imageFormat)
                      .set_depth_format(depth_image->imageFormat)
                      .set_pipeline_layout(pipeline_layout_)
                      .build_pipeline(gpu_.device);

      // Clean up shader modules after pipeline creation
      vkDestroyShaderModule(gpu_.device, vertex_shader, nullptr);
      vkDestroyShaderModule(gpu_.device, fragment_shader, nullptr);
    }

    void InfiniteGridPass::execute(const VkCommandBuffer cmd) {
      const auto color_image = registry_->get_resource<TextureHandle>("skybox_color");
      const auto depth_image = registry_->get_resource<TextureHandle>("skybox_depth");

      VkRenderingAttachmentInfo colorAttachment
          = vkinit::attachment_info(color_image->imageView, nullptr, color_image->currentLayout);

      VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(
          depth_image->imageView, nullptr, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

      VkRenderingInfo renderInfo
          = vkinit::rendering_info(color_image->getExtent2D(), &colorAttachment, &depthAttachment);

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

      gpu_.vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                                     &descriptor_write);
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
      vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(RenderConfig::GridParams), &registry_->config_.grid);
      viewport_.width = static_cast<float>(color_image->getExtent2D().width);
      viewport_.height = static_cast<float>(color_image->getExtent2D().height);
      vkCmdSetViewport(cmd, 0, 1, &viewport_);
      scissor_.extent.width = color_image->getExtent2D().width;
      scissor_.extent.height = color_image->getExtent2D().height;
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