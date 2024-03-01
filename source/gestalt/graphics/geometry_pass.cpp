#include "geometry_pass.h"

#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"

void geometry_pass::prepare() {
  fmt::print("preparing geometry pass\n");

  descriptor_layouts_.push_back(resource_manager_->per_frame_data_layout);
  descriptor_layouts_.push_back(resource_manager_->ibl_data.IblLayout);
  descriptor_layouts_.push_back(resource_manager_->material_data.resource_layout);
  descriptor_layouts_.push_back(resource_manager_->material_data.constants_layout);
  descriptor_layouts_.emplace_back(
      descriptor_layout_builder()
          .add_binding(10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
          .build(gpu_.device));

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

  
  const auto color_image = resource_manager_->get_resource<AllocatedImage>("skybox_color");
  const auto depth_image = resource_manager_->get_resource<AllocatedImage>("scene_depth");

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
  descriptor_set_
      = resource_manager_->descriptor_pool->allocate(gpu_.device, descriptor_layouts_.at(4));

  const auto color_image = resource_manager_->get_resource<AllocatedImage>("skybox_color");
  const auto depth_image = resource_manager_->get_resource<AllocatedImage>("scene_depth");

  const auto shadow_map = resource_manager_->get_resource<AllocatedImage>("directional_shadow_map");

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

  writer.clear();
  writer.write_image(
      10, shadow_map->imageView,
      resource_manager_->get_database().get_sampler(0),  // todo default_sampler_nearest
      VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.update_set(gpu_.device, descriptor_set_);
  VkDescriptorSet descriptorSets[]
      = {resource_manager_->ibl_data.IblSet, resource_manager_->material_data.resource_set,
         resource_manager_->material_data.constants_set, descriptor_set_};

  gpu_.vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                                 &descriptor_write);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 1, 4,
                          descriptorSets, 0, nullptr);

 
  viewport_.width = static_cast<float>(depth_image->getExtent2D().width);
  viewport_.height = static_cast<float>(depth_image->getExtent2D().height);
  scissor_.extent = depth_image->getExtent2D();
  vkCmdSetViewport(cmd, 0, 1, &viewport_);
  vkCmdSetScissor(cmd, 0, 1, &scissor_);

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

void deferred_pass::prepare() {
  fmt::print("preparing deferred pass\n");

  descriptor_layouts_.push_back(resource_manager_->per_frame_data_layout);
  descriptor_layouts_.push_back(resource_manager_->ibl_data.IblLayout);
  descriptor_layouts_.push_back(resource_manager_->material_data.resource_layout);
  descriptor_layouts_.push_back(resource_manager_->material_data.constants_layout);

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

  const auto gbuffer_1 = resource_manager_->get_resource<AllocatedImage>("gbuffer_1");
  const auto gbuffer_2 = resource_manager_->get_resource<AllocatedImage>("gbuffer_2");
  const auto gbuffer_3 = resource_manager_->get_resource<AllocatedImage>("gbuffer_3");
  const auto depth_image = resource_manager_->get_resource<AllocatedImage>("scene_depth");

  pipeline_ = PipelineBuilder()
                  .set_shaders(meshVertexShader, meshFragShader)
                  .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                  .set_polygon_mode(VK_POLYGON_MODE_FILL)
                  .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                  .set_multisampling_none()
                  .disable_blending(3)
                  .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
                  .set_color_attachment_formats(
                      {gbuffer_1->imageFormat, gbuffer_2->imageFormat, gbuffer_3->imageFormat})
                  .set_depth_format(depth_image->imageFormat)
                  .set_pipeline_layout(pipeline_layout_)
                  .build_pipeline(gpu_.device);

  vkDestroyShaderModule(gpu_.device, meshFragShader, nullptr);
  vkDestroyShaderModule(gpu_.device, meshVertexShader, nullptr);
}

void deferred_pass::cleanup() {
  vkDestroyPipeline(gpu_.device, pipeline_, nullptr);
}

void deferred_pass::execute(VkCommandBuffer cmd) {

  const auto gbuffer_1 = resource_manager_->get_resource<AllocatedImage>("gbuffer_1");
  const auto gbuffer_2 = resource_manager_->get_resource<AllocatedImage>("gbuffer_2");
  const auto gbuffer_3 = resource_manager_->get_resource<AllocatedImage>("gbuffer_3");
  const auto depth_image = resource_manager_->get_resource<AllocatedImage>("scene_depth");

  VkClearValue depth_clear = {.depthStencil = {1.f, 0}};
  VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(
      depth_image->imageView, &depth_clear, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  std::array attachment_begin_infos{
      vkinit::attachment_info(gbuffer_1->imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL),
      vkinit::attachment_info(gbuffer_2->imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL),
      vkinit::attachment_info(gbuffer_3->imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL)};

  VkRenderingInfo renderInfo = vkinit::rendering_info_for_gbuffer(gbuffer_1->getExtent2D(), attachment_begin_infos.data(),
                                           attachment_begin_infos.size(),
      &depthAttachment);

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

  viewport_.width = static_cast<float>(depth_image->getExtent2D().width);
  viewport_.height = static_cast<float>(depth_image->getExtent2D().height);
  scissor_.extent = depth_image->getExtent2D();
  vkCmdSetViewport(cmd, 0, 1, &viewport_);
  vkCmdSetScissor(cmd, 0, 1, &scissor_);

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
  fmt::print("preparing transparent pass\n");

  descriptor_layouts_.push_back(resource_manager_->per_frame_data_layout);
  descriptor_layouts_.push_back(resource_manager_->ibl_data.IblLayout);
  descriptor_layouts_.push_back(resource_manager_->material_data.resource_layout);
  descriptor_layouts_.push_back(resource_manager_->material_data.constants_layout);
  descriptor_layouts_.emplace_back(
      descriptor_layout_builder()
          .add_binding(10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
          .build(gpu_.device));

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
  descriptor_set_
      = resource_manager_->descriptor_pool->allocate(gpu_.device, descriptor_layouts_.at(4));

  const auto color_image = resource_manager_->get_resource<AllocatedImage>("scene_opaque_color");
  const auto depth_image = resource_manager_->get_resource<AllocatedImage>("scene_opaque_depth");

  const auto shadow_map = resource_manager_->get_resource<AllocatedImage>("directional_shadow_map");

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
         resource_manager_->material_data.constants_set, descriptor_set_};

  
  viewport_.width = static_cast<float>(depth_image->getExtent2D().width);
  viewport_.height = static_cast<float>(depth_image->getExtent2D().height);
  scissor_.extent = depth_image->getExtent2D();
  vkCmdSetViewport(cmd, 0, 1, &viewport_);
  vkCmdSetScissor(cmd, 0, 1, &scissor_);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
  writer.clear();
  writer.write_image(
      10, shadow_map->imageView,
      resource_manager_->get_database().get_sampler(0),  // todo default_sampler_nearest
      VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.update_set(gpu_.device, descriptor_set_);

  gpu_.vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                                 &descriptor_write);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 1, 4,
                          descriptorSets, 0, nullptr);

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