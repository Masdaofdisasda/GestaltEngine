#include "skybox_pass.h"

#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"
#include "vk_renderer.h"

void skybox_pass::prepare() {

  descriptor_layouts_.push_back(resource_manager_->per_frame_data_layout);
  descriptor_layouts_.push_back(resource_manager_->ibl_data.IblLayout);

  VkPipelineLayoutCreateInfo pipeline_layout_create_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .setLayoutCount = static_cast<uint32_t>(descriptor_layouts_.size()),
      .pSetLayouts = descriptor_layouts_.data(),
  };

  VK_CHECK(vkCreatePipelineLayout(gpu_.device, &pipeline_layout_create_info, nullptr,
                                  &pipeline_layout_));

  VkShaderModule vertex_shader;
  vkutil::load_shader_module(vertex_shader_source_.c_str(), gpu_.device, &vertex_shader);
  VkShaderModule fragment_shader;
  vkutil::load_shader_module(fragment_shader_source_.c_str(), gpu_.device, &fragment_shader);

  pipeline_ = PipelineBuilder()
                  .set_shaders(vertex_shader, fragment_shader)
                  .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                  .set_polygon_mode(VK_POLYGON_MODE_FILL)
                  .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                  .set_multisampling_none()
                  .disable_blending()
                  .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
                  .set_color_attachment_format(renderer_->frame_buffer_.get_write_color_image().imageFormat)
                  .set_depth_format(renderer_->frame_buffer_.get_write_depth_image().imageFormat)
                  .set_pipeline_layout(pipeline_layout_)
                  .build_pipeline(gpu_.device);

  // Clean up shader modules after pipeline creation
  vkDestroyShaderModule(gpu_.device, vertex_shader, nullptr);
  vkDestroyShaderModule(gpu_.device, fragment_shader, nullptr);
}

void skybox_pass::execute(const VkCommandBuffer cmd) {

  vkutil::transition_write(cmd, renderer_->frame_buffer_.get_write_color_image());
  vkutil::transition_write(cmd, renderer_->frame_buffer_.get_write_depth_image());

  VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(renderer_->frame_buffer_.get_write_color_image().imageView, nullptr,
                                renderer_->frame_buffer_.get_write_color_image().currentLayout);
  VkClearValue depth_clear = {.depthStencil = {1.f, 0}};
  VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(
      renderer_->frame_buffer_.get_write_depth_image().imageView, &depth_clear, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  VkRenderingInfo renderInfo
      = vkinit::rendering_info(renderer_->frame_buffer_.get_write_color_image().getExtent2D(),
                               &colorAttachment, &depthAttachment);

  vkCmdBeginRendering(cmd, &renderInfo);

  VkDescriptorBufferInfo buffer_info;
  buffer_info.buffer = resource_manager_->per_frame_data_buffer.buffer;
  buffer_info.offset = 0;
  buffer_info.range = sizeof(per_frame_data);

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
                          &resource_manager_->ibl_data.IblSet, 0, nullptr);
  VkViewport viewport = {};
  viewport.x = 0;
  viewport.y = 0;
  viewport.width
      = static_cast<float>(renderer_->frame_buffer_.get_write_color_image().getExtent2D().width);
  viewport.height
      = static_cast<float>(renderer_->frame_buffer_.get_write_color_image().getExtent2D().height);
  viewport.minDepth = 0.f;
  viewport.maxDepth = 1.f;

  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = {};
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent.width = renderer_->frame_buffer_.get_write_color_image().getExtent2D().width;
  scissor.extent.height = renderer_->frame_buffer_.get_write_color_image().getExtent2D().height;

  vkCmdSetScissor(cmd, 0, 1, &scissor);
  vkCmdDraw(cmd, 36, 1, 0, 0);  // 36 vertices for the cube

  vkCmdEndRendering(cmd);
}

void skybox_pass::cleanup() {
  vkDestroyPipelineLayout(gpu_.device, pipeline_layout_, nullptr);
  vkDestroyPipeline(gpu_.device, pipeline_, nullptr);
}

void skybox_shader::prepare() {

  descriptor_layouts_.push_back(resource_manager_->per_frame_data_layout);
  descriptor_layouts_.push_back(resource_manager_->ibl_data.IblLayout);

  VkPipelineLayoutCreateInfo pipeline_layout_create_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .setLayoutCount = static_cast<uint32_t>(descriptor_layouts_.size()),
      .pSetLayouts = descriptor_layouts_.data(),
  };

  VK_CHECK(vkCreatePipelineLayout(gpu_.device, &pipeline_layout_create_info, nullptr,
                                  &pipeline_layout_));

  VkShaderModule vertex_shader;
  vkutil::load_shader_module(vertex_shader_source_.c_str(), gpu_.device, &vertex_shader);
  VkShaderModule fragment_shader;
  vkutil::load_shader_module(fragment_shader_source_.c_str(), gpu_.device, &fragment_shader);

  const auto color_image = resource_manager_->get_resource<AllocatedImage>("scene_color");
  const auto depth_image = resource_manager_->get_resource<AllocatedImage>("scene_depth");

  pipeline_ = PipelineBuilder()
                  .set_shaders(vertex_shader, fragment_shader)
                  .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                  .set_polygon_mode(VK_POLYGON_MODE_FILL)
                  .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                  .set_multisampling_none()
                  .disable_blending()
                  .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
                  .set_color_attachment_format(color_image->imageFormat)
                  .set_depth_format(depth_image->imageFormat)
                  .set_pipeline_layout(pipeline_layout_)
                  .build_pipeline(gpu_.device);

  // Clean up shader modules after pipeline creation
  vkDestroyShaderModule(gpu_.device, vertex_shader, nullptr);
  vkDestroyShaderModule(gpu_.device, fragment_shader, nullptr);
}

void skybox_shader::execute(const VkCommandBuffer cmd) {
  const auto color_image = resource_manager_->get_resource<AllocatedImage>("scene_color");
  const auto depth_image = resource_manager_->get_resource<AllocatedImage>("scene_depth");

  VkRenderingAttachmentInfo colorAttachment
      = vkinit::attachment_info(color_image->imageView, nullptr, color_image->currentLayout);
  VkClearValue depth_clear = {.depthStencil = {1.f, 0}};
  VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(
      depth_image->imageView, &depth_clear, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  VkRenderingInfo renderInfo
      = vkinit::rendering_info(color_image->getExtent2D(),
                               &colorAttachment, &depthAttachment);

  vkCmdBeginRendering(cmd, &renderInfo);

  VkDescriptorBufferInfo buffer_info;
  buffer_info.buffer = resource_manager_->per_frame_data_buffer.buffer;
  buffer_info.offset = 0;
  buffer_info.range = sizeof(per_frame_data);

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
                          &resource_manager_->ibl_data.IblSet, 0, nullptr);
  VkViewport viewport = {};
  viewport.x = 0;
  viewport.y = 0;
  viewport.width = static_cast<float>(color_image->getExtent2D().width);
  viewport.height = static_cast<float>(color_image->getExtent2D().height);
  viewport.minDepth = 0.f;
  viewport.maxDepth = 1.f;

  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = {};
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent.width = color_image->getExtent2D().width;
  scissor.extent.height = color_image->getExtent2D().height;

  vkCmdSetScissor(cmd, 0, 1, &scissor);
  vkCmdDraw(cmd, 36, 1, 0, 0);  // 36 vertices for the cube

  vkCmdEndRendering(cmd);
}

void skybox_shader::cleanup() {
  vkDestroyPipelineLayout(gpu_.device, pipeline_layout_, nullptr);
  vkDestroyPipeline(gpu_.device, pipeline_, nullptr);
}