#include "lighting_pass.h"

#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"

void lighting_pass::prepare() {
  fmt::print("preparing lighting pass\n");

  descriptor_layouts_.push_back(resource_manager_->per_frame_data_layout);
  descriptor_layouts_.push_back(resource_manager_->ibl_data.IblLayout);
  descriptor_layouts_.emplace_back(
      descriptor_layout_builder()
          .add_binding(10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
          .add_binding(11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
          .add_binding(12, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
          .add_binding(13, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
          .add_binding(14, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
          .add_binding(15, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, false,
                       resource_manager_->get_database().max_lights(light_type::directional))
          .add_binding(16, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, false,
                       resource_manager_->get_database().max_lights(light_type::point))
          .add_binding(17, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, false,
                       resource_manager_->get_database()
                           .max_lights(light_type::directional) +
                       resource_manager_->get_database().max_lights(light_type::point))
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

  const auto color_image = resource_manager_->get_resource<AllocatedImage>("gbuffer_shaded");

  pipeline_ = PipelineBuilder()
                  .set_shaders(meshVertexShader, meshFragShader)
                  .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                  .set_polygon_mode(VK_POLYGON_MODE_FILL)
                  .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                  .set_multisampling_none()
                  .disable_blending()
                  .set_color_attachment_format(color_image->imageFormat)
                  .disable_depthtest()
                  .set_pipeline_layout(pipeline_layout_)
                  .build_pipeline(gpu_.device);

  vkDestroyShaderModule(gpu_.device, meshFragShader, nullptr);
  vkDestroyShaderModule(gpu_.device, meshVertexShader, nullptr);
}

void lighting_pass::cleanup() {
  vkDestroyPipeline(gpu_.device, pipeline_, nullptr);
}

void lighting_pass::execute(VkCommandBuffer cmd) {
  descriptor_set_
      = resource_manager_->descriptor_pool->allocate(gpu_.device, descriptor_layouts_.at(2));

  const auto color_image = resource_manager_->get_resource<AllocatedImage>("gbuffer_shaded");

  const auto gbuffer_1 = resource_manager_->get_resource<AllocatedImage>("gbuffer_1_final");
  const auto gbuffer_2 = resource_manager_->get_resource<AllocatedImage>("gbuffer_2_final");
  const auto gbuffer_3 = resource_manager_->get_resource<AllocatedImage>("gbuffer_3_final");
  const auto gbuffer_depth = resource_manager_->get_resource<AllocatedImage>("gbuffer_depth");
  const auto shadow_map = resource_manager_->get_resource<AllocatedImage>("directional_shadow_map");

  VkRenderingAttachmentInfo colorAttachment
      = vkinit::attachment_info(color_image->imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);

  VkRenderingInfo renderInfo
      = vkinit::rendering_info(color_image->getExtent2D(), &colorAttachment, nullptr);

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

  writer.clear();
  writer.write_image(
      10, gbuffer_1->imageView,
      resource_manager_->get_database().get_sampler(0),  // todo default_sampler_nearest
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.write_image(
      11, gbuffer_2->imageView,
      resource_manager_->get_database().get_sampler(0),  // todo default_sampler_nearest
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.write_image(
      12, gbuffer_3->imageView,
      resource_manager_->get_database().get_sampler(0),  // todo default_sampler_nearest
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.write_image(
      13, gbuffer_depth->imageView,
      resource_manager_->get_database().get_sampler(0),  // todo default_sampler_nearest
      VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.write_image(
      14, shadow_map->imageView,
      resource_manager_->get_database().get_sampler(0),  // todo default_sampler_nearest
      VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

  std::vector<VkDescriptorBufferInfo> dirBufferInfos;
  for (int i = 0; i < resource_manager_->get_database().max_lights(light_type::directional); ++i) {
       VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = resource_manager_->light_data.dir_light_buffer.buffer;
       bufferInfo.offset = 32 * i;
       bufferInfo.range = 32;
       dirBufferInfos.push_back(bufferInfo);
  }
  writer.write_buffer_array(15, dirBufferInfos, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);

  std::vector<VkDescriptorBufferInfo> pointBufferInfos;
  for (int i = 0; i < resource_manager_->get_database().max_lights(light_type::point); ++i) {
       VkDescriptorBufferInfo bufferInfo = {};
       bufferInfo.buffer = resource_manager_->light_data.point_light_buffer.buffer;
       bufferInfo.offset = 32 * i;
       bufferInfo.range = 32;
       pointBufferInfos.push_back(bufferInfo);
  }
  writer.write_buffer_array(16, pointBufferInfos, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);

  std::vector<VkDescriptorBufferInfo> lightViewProjBufferInfos;
  for (int i = 0; i < resource_manager_->get_database()
                          .max_lights(light_type::directional)  + resource_manager_->get_database()
                          .max_lights(light_type::point);
       ++i) {
       VkDescriptorBufferInfo bufferInfo = {};
       bufferInfo.buffer = resource_manager_->light_data.view_proj_matrices.buffer;
       bufferInfo.offset = 64 * i;
       bufferInfo.range = 64;
       lightViewProjBufferInfos.push_back(bufferInfo);
  }
  writer.write_buffer_array(17, lightViewProjBufferInfos, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);

  writer.update_set(gpu_.device, descriptor_set_);
  VkDescriptorSet descriptorSets[] = {resource_manager_->ibl_data.IblSet, descriptor_set_};

  gpu_.vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                                 &descriptor_write);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 1, 2,
                          descriptorSets, 0, nullptr);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
  viewport_.width = static_cast<float>(color_image->getExtent2D().width);
  viewport_.height = static_cast<float>(color_image->getExtent2D().height);
  scissor_.extent.width = color_image->getExtent2D().width;
  scissor_.extent.height = color_image->getExtent2D().height;
  vkCmdSetViewport(cmd, 0, 1, &viewport_);
  vkCmdSetScissor(cmd, 0, 1, &scissor_);
  resource_manager_->config_.lighting.invViewProj
      = inverse(resource_manager_->per_frame_data_.viewproj);

  vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                     sizeof(render_config::lighting_params), &resource_manager_->config_.lighting);
  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdEndRendering(cmd);
}