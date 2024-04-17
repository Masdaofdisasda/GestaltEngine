#include "SsaoPass.h"

#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"

namespace gestalt {
  namespace graphics {
      /*
    using namespace foundation;

    void SsaoFilterPass::prepare() {
      fmt::print("Preparing ssao filter pass\n");
      rotation_pattern = resource_manager_->load_image("../../assets/rot_texture.bmp").value();

      descriptor_layouts_.emplace_back(
          DescriptorLayoutBuilder()
              .add_binding(10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_FRAGMENT_BIT)
              .add_binding(11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_FRAGMENT_BIT)
              .build(gpu_.device));
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
      VkShaderModule ssao_filter_shader;
      vkutil::load_shader_module(fragment_shader_source_.c_str(), gpu_.device, &ssao_filter_shader);

      const auto color_image = registry_->get_resource<TextureHandle>("color_ssao_filter");

      pipeline_ = PipelineBuilder()
                      .set_shaders(vertex_shader, ssao_filter_shader)
                      .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                      .set_polygon_mode(VK_POLYGON_MODE_FILL)
                      .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                      .set_multisampling_none()
                      .disable_blending()
                      .disable_depthtest()
                      .set_color_attachment_format(color_image->getFormat())
                      .set_pipeline_layout(pipeline_layout_)
                      .build_pipeline(gpu_.device);
    }

    void SsaoFilterPass::execute(VkCommandBuffer cmd) {
      descriptor_set_
          = resource_manager_->descriptor_pool->allocate(gpu_.device, descriptor_layouts_.at(0));

      const auto scene_depth = registry_->get_resource<TextureHandle>("grid_depth");
      const auto color_image = registry_->get_resource<TextureHandle>("color_ssao_filter");

      VkRenderingAttachmentInfo newColorAttachment = vkinit::attachment_info(
          color_image->imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      VkRenderingInfo newRenderInfo
          = vkinit::rendering_info({effect_size_, effect_size_}, &newColorAttachment, nullptr);
      vkCmdBeginRendering(cmd, &newRenderInfo);

      writer.clear();
      writer.write_image(10, scene_depth->imageView, repository_->default_material_.nearestSampler,
                         VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.write_image(
          11, rotation_pattern.imageView, repository_->default_material_.nearestSampler,
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.update_set(gpu_.device, descriptor_set_);

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                              &descriptor_set_, 0, nullptr);
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

      vkCmdSetViewport(cmd, 0, 1, &viewport_);
      vkCmdSetScissor(cmd, 0, 1, &scissor_);

      vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(RenderConfig::SsaoParams), &registry_->config_.ssao);
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRendering(cmd);
    }

    void SsaoFilterPass::cleanup() {
      vkDestroyPipelineLayout(gpu_.device, pipeline_layout_, nullptr);
      vkDestroyPipeline(gpu_.device, pipeline_, nullptr);
    }

    void SsaoBlurPass::prepare() {
      fmt::print("Preparing ssao blur pass\n");

      descriptor_layouts_.emplace_back(DescriptorLayoutBuilder()
                                           .add_binding(10,
                                                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT)
                                           .build(gpu_.device));
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
      VkShaderModule ssao_blur_x_shader;
      vkutil::load_shader_module(fragment_blur_x.c_str(), gpu_.device, &ssao_blur_x_shader);
      VkShaderModule ssao_blur_y_shader;
      vkutil::load_shader_module(fragment_blur_y.c_str(), gpu_.device, &ssao_blur_y_shader);

      const auto color_image = registry_->get_resource<TextureHandle>("color_ssao_filtered");

      PipelineBuilder builder
          = PipelineBuilder()
                .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                .set_polygon_mode(VK_POLYGON_MODE_FILL)
                .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                .set_multisampling_none()
                .disable_blending()
                .disable_depthtest()
                .set_color_attachment_format(color_image->getFormat())
                .set_pipeline_layout(pipeline_layout_);

      blur_x_pipeline_
          = builder.set_shaders(vertex_shader, ssao_blur_x_shader).build_pipeline(gpu_.device);
      blur_y_pipeline_
          = builder.set_shaders(vertex_shader, ssao_blur_y_shader).build_pipeline(gpu_.device);
    }

    void SsaoBlurPass::execute(VkCommandBuffer cmd) {
      const auto image_x = registry_->get_resource<TextureHandle>("color_ssao_filtered");
      const auto image_y = registry_->get_resource<TextureHandle>("ssao_blur_y");

      for (int i = 0; i < registry_->config_.ssao_quality; ++i) {
        {
          blur_x_descriptor_set_ = resource_manager_->descriptor_pool->allocate(
              gpu_.device, descriptor_layouts_.at(0));

          vkutil::TransitionImage(image_x).toLayoutRead().andSubmitTo(cmd);
          vkutil::TransitionImage(image_y).toLayoutWrite().andSubmitTo(cmd);

          VkRenderingAttachmentInfo newColorAttachment = vkinit::attachment_info(
              image_y->imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
          VkRenderingInfo newRenderInfo
              = vkinit::rendering_info({effect_size_, effect_size_}, &newColorAttachment, nullptr);
          vkCmdBeginRendering(cmd, &newRenderInfo);

          writer.clear();
          writer.write_image(10, image_x->imageView, repository_->default_material_.nearestSampler,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
          writer.update_set(gpu_.device, blur_x_descriptor_set_);

          vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                                  &blur_x_descriptor_set_, 0, nullptr);
          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blur_x_pipeline_);

          vkCmdSetViewport(cmd, 0, 1, &viewport_);
          vkCmdSetScissor(cmd, 0, 1, &scissor_);
          vkCmdDraw(cmd, 3, 1, 0, 0);
          vkCmdEndRendering(cmd);
        }
        {
          blur_y_descriptor_set_ = resource_manager_->descriptor_pool->allocate(
              gpu_.device, descriptor_layouts_.at(0));

          vkutil::TransitionImage(image_y).toLayoutRead().andSubmitTo(cmd);
          vkutil::TransitionImage(image_x).toLayoutWrite().andSubmitTo(cmd);

          VkRenderingAttachmentInfo newColorAttachment = vkinit::attachment_info(
              image_x->imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
          VkRenderingInfo newRenderInfo
              = vkinit::rendering_info({effect_size_, effect_size_}, &newColorAttachment, nullptr);
          vkCmdBeginRendering(cmd, &newRenderInfo);

          writer.clear();
          writer.write_image(10, image_y->imageView, repository_->default_material_.nearestSampler,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
          writer.update_set(gpu_.device, blur_y_descriptor_set_);

          vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                                  &blur_y_descriptor_set_, 0, nullptr);
          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blur_y_pipeline_);

          vkCmdSetViewport(cmd, 0, 1, &viewport_);
          vkCmdSetScissor(cmd, 0, 1, &scissor_);
          vkCmdDraw(cmd, 3, 1, 0, 0);
          vkCmdEndRendering(cmd);
        }
      }
    }

    void SsaoBlurPass::cleanup() {
      vkDestroyPipelineLayout(gpu_.device, pipeline_layout_, nullptr);
      vkDestroyPipeline(gpu_.device, blur_x_pipeline_, nullptr);
      vkDestroyPipeline(gpu_.device, blur_y_pipeline_, nullptr);
    }

    void SsaoFinalPass::prepare() {
      fmt::print("Preparing ssao final pass\n");

      descriptor_layouts_.emplace_back(
          DescriptorLayoutBuilder()
              .add_binding(10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_FRAGMENT_BIT)
              .add_binding(11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_FRAGMENT_BIT)
              .build(gpu_.device));
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
      VkShaderModule ssao_final_shader;
      vkutil::load_shader_module(fragment_shader_source_.c_str(), gpu_.device, &ssao_final_shader);

      const auto color_image = registry_->get_resource<TextureHandle>("scene_ssao_final");

      pipeline_ = PipelineBuilder()
                      .set_shaders(vertex_shader, ssao_final_shader)
                      .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                      .set_polygon_mode(VK_POLYGON_MODE_FILL)
                      .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                      .set_multisampling_none()
                      .disable_blending()
                      .disable_depthtest()
                      .set_color_attachment_format(color_image->getFormat())
                      .set_pipeline_layout(pipeline_layout_)
                      .build_pipeline(gpu_.device);
    }

    void SsaoFinalPass::execute(VkCommandBuffer cmd) {
      descriptor_set_
          = resource_manager_->descriptor_pool->allocate(gpu_.device, descriptor_layouts_.at(0));

      const auto scene_color = registry_->get_resource<TextureHandle>("grid_color");
      const auto scene_ssao = registry_->get_resource<TextureHandle>("scene_ssao_final");
      const auto ssao_blurred = registry_->get_resource<TextureHandle>("ssao_blurred_final");

      VkRenderingAttachmentInfo newColorAttachment = vkinit::attachment_info(
          scene_ssao->imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      VkRenderingInfo newRenderInfo
          = vkinit::rendering_info(scene_ssao->getExtent2D(), &newColorAttachment, nullptr);
      vkCmdBeginRendering(cmd, &newRenderInfo);

      writer.clear();
      writer.write_image(10, scene_color->imageView, repository_->default_material_.nearestSampler,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.write_image(11, ssao_blurred->imageView, repository_->default_material_.nearestSampler,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.update_set(gpu_.device, descriptor_set_);

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                              &descriptor_set_, 0, nullptr);
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
      viewport_.width = static_cast<float>(scene_ssao->getExtent2D().width);
      viewport_.height = static_cast<float>(scene_ssao->getExtent2D().height);
      scissor_.extent.width = scene_ssao->getExtent2D().width;
      scissor_.extent.height = scene_ssao->getExtent2D().height;
      vkCmdSetViewport(cmd, 0, 1, &viewport_);
      vkCmdSetScissor(cmd, 0, 1, &scissor_);
      vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(RenderConfig::SsaoParams), &registry_->config_.ssao);
      vkCmdDraw(cmd, 3, 1, 0, 0);
      vkCmdEndRendering(cmd);
    }

    void SsaoFinalPass::cleanup() {
      vkDestroyPipelineLayout(gpu_.device, pipeline_layout_, nullptr);
      vkDestroyPipeline(gpu_.device, pipeline_, nullptr);
    }*/
  }  // namespace graphics
}  // namespace gestalt