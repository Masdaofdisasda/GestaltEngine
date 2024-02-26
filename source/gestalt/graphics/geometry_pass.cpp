#include "geometry_pass.h"

#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"

void geometry_pass::prepare() {

  descriptor_layouts_.push_back(resource_manager_->per_frame_data_layout);
  descriptor_layouts_.push_back(resource_manager_->ibl_data.IblLayout);
  descriptor_layouts_.push_back(resource_manager_->material_data.resource_layout);
  descriptor_layouts_.push_back(resource_manager_->material_data.constants_layout);

  VkShaderModule meshFragShader;
  vkutil::load_shader_module(fragment_shader_source_.c_str(), gpu_.device, &meshFragShader);

  VkShaderModule meshVertexShader;
  vkutil::load_shader_module(vertex_shader_source_.c_str(), gpu_.device, &meshVertexShader);

  VkPushConstantRange matrix_range{};
  matrix_range.offset = 0;
  matrix_range.size = sizeof(GPUDrawPushConstants);
  matrix_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
  mesh_layout_info.setLayoutCount = descriptor_layouts_.size();
  mesh_layout_info.pSetLayouts = descriptor_layouts_.data();
  mesh_layout_info.pPushConstantRanges = &matrix_range;
  mesh_layout_info.pushConstantRangeCount = 1;

  VK_CHECK(vkCreatePipelineLayout(gpu_.device, &mesh_layout_info, nullptr, &pipeline_layout_));

  
  const auto color_image = resource_manager_->get_resource<AllocatedImage>("skybox_color");
  const auto depth_image = resource_manager_->get_resource<AllocatedImage>("skybox_depth");

  pipeline_ = PipelineBuilder()
            .set_shaders(meshVertexShader, meshFragShader)
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

  vkDestroyShaderModule(gpu_.device, meshFragShader, nullptr);
  vkDestroyShaderModule(gpu_.device, meshVertexShader, nullptr);
}

void geometry_pass::cleanup() {
  vkDestroyPipeline(gpu_.device, pipeline_, nullptr);
}

void geometry_pass::execute(VkCommandBuffer cmd) {
  const auto color_image = resource_manager_->get_resource<AllocatedImage>("skybox_color");
  const auto depth_image = resource_manager_->get_resource<AllocatedImage>("skybox_depth");

  VkRenderingAttachmentInfo colorAttachment
      = vkinit::attachment_info(color_image->imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
  VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(
      depth_image->imageView, nullptr, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  VkRenderingInfo renderInfo
      = vkinit::rendering_info(color_image->getExtent2D(), &colorAttachment, &depthAttachment);

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

  vkCmdBindIndexBuffer(cmd, resource_manager_->scene_geometry_.indexBuffer.buffer, 0,
                       VK_INDEX_TYPE_UINT32);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
  VkDescriptorSet descriptorSets[]
      = {resource_manager_->ibl_data.IblSet, resource_manager_->material_data.resource_set,
         resource_manager_->material_data.constants_set};

  gpu_.vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                                 &descriptor_write);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 1, 3,
                          descriptorSets, 0, nullptr);

  const VkViewport viewport = {
      .x = 0,
      .y = 0,
      .width = static_cast<float>(color_image->getExtent2D().width),
      .height = static_cast<float>(color_image->getExtent2D().height),
      .minDepth = 0.f,
      .maxDepth = 1.f,
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = {};
  scissor.offset.y = 0;
  scissor.offset.x = 0;
  scissor.extent.width = color_image->getExtent2D().width;
  scissor.extent.height = color_image->getExtent2D().height;
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  for (auto& r : resource_manager_->main_draw_context_.opaque_surfaces) {
    GPUDrawPushConstants push_constants;
    push_constants.worldMatrix = r.transform;
    push_constants.material_id = r.material;
    push_constants.vertexBuffer = r.vertex_buffer_address;
    vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(GPUDrawPushConstants), &push_constants);
    vkCmdDrawIndexed(cmd, r.index_count, 1, r.first_index, 0, 0);
  }
  if (resource_manager_->config_.always_opaque) {
    for (auto& r : resource_manager_->main_draw_context_.transparent_surfaces) {
           GPUDrawPushConstants push_constants;
           push_constants.worldMatrix = r.transform;
           push_constants.material_id = r.material;
           push_constants.vertexBuffer = r.vertex_buffer_address;
           vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                              sizeof(GPUDrawPushConstants), &push_constants);
           vkCmdDrawIndexed(cmd, r.index_count, 1, r.first_index, 0, 0);
    }
  }

  vkCmdEndRendering(cmd);
}
void transparent_pass::prepare() {
  descriptor_layouts_.push_back(resource_manager_->per_frame_data_layout);
  descriptor_layouts_.push_back(resource_manager_->ibl_data.IblLayout);
  descriptor_layouts_.push_back(resource_manager_->material_data.resource_layout);
  descriptor_layouts_.push_back(resource_manager_->material_data.constants_layout);

  VkShaderModule meshFragShader;
  vkutil::load_shader_module(fragment_shader_source_.c_str(), gpu_.device, &meshFragShader);

  VkShaderModule meshVertexShader;
  vkutil::load_shader_module(vertex_shader_source_.c_str(), gpu_.device, &meshVertexShader);

  VkPushConstantRange matrix_range{};
  matrix_range.offset = 0;
  matrix_range.size = sizeof(GPUDrawPushConstants);
  matrix_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
  mesh_layout_info.setLayoutCount = descriptor_layouts_.size();
  mesh_layout_info.pSetLayouts = descriptor_layouts_.data();
  mesh_layout_info.pPushConstantRanges = &matrix_range;
  mesh_layout_info.pushConstantRangeCount = 1;

  VK_CHECK(vkCreatePipelineLayout(gpu_.device, &mesh_layout_info, nullptr, &pipeline_layout_));

  const auto color_image = resource_manager_->get_resource<AllocatedImage>("scene_opaque_color");
  const auto depth_image = resource_manager_->get_resource<AllocatedImage>("scene_opaque_depth");

  pipeline_ = PipelineBuilder()
                  .set_shaders(meshVertexShader, meshFragShader)
                  .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                  .set_polygon_mode(VK_POLYGON_MODE_FILL)
                  .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                  .set_multisampling_none()
                  .enable_blending_additive()
                  .enable_depthtest(false, VK_COMPARE_OP_LESS_OR_EQUAL)
                  .set_color_attachment_format(color_image->imageFormat)
                  .set_depth_format(depth_image->imageFormat)
                  .set_pipeline_layout(pipeline_layout_)
                  .build_pipeline(gpu_.device);

  vkDestroyShaderModule(gpu_.device, meshFragShader, nullptr);
  vkDestroyShaderModule(gpu_.device, meshVertexShader, nullptr);
}

void transparent_pass::execute(VkCommandBuffer cmd) {
  const auto color_image = resource_manager_->get_resource<AllocatedImage>("scene_opaque_color");
  const auto depth_image = resource_manager_->get_resource<AllocatedImage>("scene_opaque_depth");

  VkRenderingAttachmentInfo colorAttachment
      = vkinit::attachment_info(color_image->imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
  VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(
      depth_image->imageView, nullptr, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  VkRenderingInfo renderInfo
      = vkinit::rendering_info(color_image->getExtent2D(), &colorAttachment, &depthAttachment);

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

  vkCmdBindIndexBuffer(cmd, resource_manager_->scene_geometry_.indexBuffer.buffer, 0,
                       VK_INDEX_TYPE_UINT32);

  VkDescriptorSet descriptorSets[]
      = {resource_manager_->ibl_data.IblSet, resource_manager_->material_data.resource_set,
         resource_manager_->material_data.constants_set};

  const VkViewport viewport = {
      .x = 0,
      .y = 0,
      .width = static_cast<float>(color_image->getExtent2D().width),
      .height = static_cast<float>(color_image->getExtent2D().height),
      .minDepth = 0.f,
      .maxDepth = 1.f,
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = {};
  scissor.offset.y = 0;
  scissor.offset.x = 0;
  scissor.extent.width = color_image->getExtent2D().width;
  scissor.extent.height = color_image->getExtent2D().height;
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

  gpu_.vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                                 &descriptor_write);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 1, 3,
                          descriptorSets, 0, nullptr);

  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  for (auto& r : resource_manager_->main_draw_context_.transparent_surfaces) {
    GPUDrawPushConstants push_constants;
    push_constants.worldMatrix = r.transform;
    push_constants.material_id = r.material;
    push_constants.vertexBuffer = r.vertex_buffer_address;
    vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(GPUDrawPushConstants), &push_constants);
    vkCmdDrawIndexed(cmd, r.index_count, 1, r.first_index, 0, 0);
  }

  vkCmdEndRendering(cmd);
}

void transparent_pass::cleanup() { vkDestroyPipeline(gpu_.device, pipeline_, nullptr); }