#include "LightingPass.h"

#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"


namespace gestalt {
  namespace graphics {

    using namespace foundation;
    void LightingPass::prepare() {
      fmt::print("preparing lighting pass\n");

      descriptor_layouts_.push_back(resource_manager_->per_frame_data_layout);
      descriptor_layouts_.push_back(resource_manager_->ibl_data.IblLayout);
      descriptor_layouts_.emplace_back(
          DescriptorLayoutBuilder()
              .add_binding(10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_FRAGMENT_BIT)
              .add_binding(11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_FRAGMENT_BIT)
              .add_binding(12, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_FRAGMENT_BIT)
              .add_binding(13, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_FRAGMENT_BIT)
              .add_binding(14, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_FRAGMENT_BIT)
              .build(gpu_.device));
      descriptor_layouts_.push_back(resource_manager_->light_data.light_layout);

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

      const auto color_image = registry_->get_resource<TextureHandle>("gbuffer_shaded");

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

    void LightingPass::cleanup() { vkDestroyPipeline(gpu_.device, pipeline_, nullptr); }

    void LightingPass::execute(VkCommandBuffer cmd) {
      descriptor_set_
          = resource_manager_->descriptor_pool->allocate(gpu_.device, descriptor_layouts_.at(2));

      const auto color_image = registry_->get_resource<TextureHandle>("gbuffer_shaded");

      const auto gbuffer_1 = registry_->get_resource<TextureHandle>("gbuffer_1_final");
      const auto gbuffer_2 = registry_->get_resource<TextureHandle>("gbuffer_2_final");
      const auto gbuffer_3 = registry_->get_resource<TextureHandle>("gbuffer_3_final");
      const auto gbuffer_depth = registry_->get_resource<TextureHandle>("gbuffer_depth");
      const auto shadow_map = registry_->get_resource<TextureHandle>("directional_shadow_map");

      VkRenderingAttachmentInfo colorAttachment
          = vkinit::attachment_info(color_image->imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);

      VkRenderingInfo renderInfo
          = vkinit::rendering_info(color_image->getExtent2D(), &colorAttachment, nullptr);

      vkCmdBeginRendering(cmd, &renderInfo);

      VkDescriptorBufferInfo buffer_info;
      buffer_info.buffer = resource_manager_->per_frame_data_buffer.buffer;
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

      writer.clear();
      writer.write_image(10, gbuffer_1->imageView, repository_->default_material_.nearestSampler,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.write_image(11, gbuffer_2->imageView, repository_->default_material_.nearestSampler,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.write_image(12, gbuffer_3->imageView, repository_->default_material_.nearestSampler,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.write_image(
          13, gbuffer_depth->imageView, repository_->default_material_.nearestSampler,
          VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.write_image(14, shadow_map->imageView, repository_->default_material_.nearestSampler,
                         VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.update_set(gpu_.device, descriptor_set_);
      VkDescriptorSet descriptorSets[] = {resource_manager_->ibl_data.IblSet, descriptor_set_,
                                          resource_manager_->light_data.light_set};

      gpu_.vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                                     &descriptor_write);
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 1, 3,
                              descriptorSets, 0, nullptr);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
      viewport_.width = static_cast<float>(color_image->getExtent2D().width);
      viewport_.height = static_cast<float>(color_image->getExtent2D().height);
      scissor_.extent.width = color_image->getExtent2D().width;
      scissor_.extent.height = color_image->getExtent2D().height;
      vkCmdSetViewport(cmd, 0, 1, &viewport_);
      vkCmdSetScissor(cmd, 0, 1, &scissor_);
      registry_->config_.lighting.invViewProj
          = glm::inverse(registry_->per_frame_data_.viewproj);
      registry_->config_.lighting.num_dir_lights = repository_->directional_lights.size();
      registry_->config_.lighting.num_point_lights = repository_->point_lights.size();

      vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(RenderConfig::LightingParams), &registry_->config_.lighting);
      vkCmdDraw(cmd, 3, 1, 0, 0);
      vkCmdEndRendering(cmd);
    }
  }  // namespace graphics
}  // namespace gestalt