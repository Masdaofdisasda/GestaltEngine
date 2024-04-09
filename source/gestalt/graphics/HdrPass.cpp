#include "HdrPass.h"

#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"

namespace gestalt {
  namespace graphics {

    using namespace foundation;
    void BrightPass::prepare() {
      fmt::print("Preparing bright pass\n");

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
          .pushConstantRangeCount = 1,
          .pPushConstantRanges = &push_constant_range,
      };
      VK_CHECK(vkCreatePipelineLayout(gpu_.device, &pipeline_layout_create_info, nullptr,
                                      &pipeline_layout_));

      VkShaderModule vertex_shader;
      vkutil::load_shader_module(vertex_shader_source_.c_str(), gpu_.device, &vertex_shader);
      VkShaderModule bright_pass_shader;
      vkutil::load_shader_module(fragment_shader_source_.c_str(), gpu_.device, &bright_pass_shader);

      const auto color_image = registry_->get_resource<TextureHandle>("scene_bright");

      pipeline_ = PipelineBuilder()
                      .set_shaders(vertex_shader, bright_pass_shader)
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

    void BrightPass::execute(VkCommandBuffer cmd) {
      descriptor_set_
          = resource_manager_->descriptor_pool->allocate(gpu_.device, descriptor_layouts_.at(0));

      const auto scene_ssao = registry_->get_resource<TextureHandle>("scene_ssao");
      const auto color_image = registry_->get_resource<TextureHandle>("scene_bright");

      VkRenderingAttachmentInfo newColorAttachment
          = vkinit::attachment_info(color_image->imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
      VkRenderingInfo newRenderInfo
          = vkinit::rendering_info({effect_size_, effect_size_}, &newColorAttachment, nullptr);
      vkCmdBeginRendering(cmd, &newRenderInfo);

      writer.clear();
      writer.write_image(10, scene_ssao->imageView, repository_->default_material_.nearestSampler,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.update_set(gpu_.device, descriptor_set_);

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                              &descriptor_set_, 0, nullptr);
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

      vkCmdSetViewport(cmd, 0, 1, &viewport_);
      vkCmdSetScissor(cmd, 0, 1, &scissor_);
      vkCmdDraw(cmd, 3, 1, 0, 0);
      vkCmdEndRendering(cmd);
    }

    void BrightPass::cleanup() {
      vkDestroyPipelineLayout(gpu_.device, pipeline_layout_, nullptr);
      vkDestroyPipeline(gpu_.device, pipeline_, nullptr);
    }

    void BloomBlurPass::prepare() {
      fmt::print("Preparing bloom blur pass\n");

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
      VkShaderModule blur_x_shader;
      vkutil::load_shader_module(fragment_blur_x.c_str(), gpu_.device, &blur_x_shader);
      VkShaderModule blur_y_shader;
      vkutil::load_shader_module(fragment_blur_y.c_str(), gpu_.device, &blur_y_shader);

      const auto color_image = registry_->get_resource<TextureHandle>("scene_brightness_filtered");

      auto builder = PipelineBuilder()
                         .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                         .set_polygon_mode(VK_POLYGON_MODE_FILL)
                         .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                         .set_multisampling_none()
                         .disable_blending()
                         .disable_depthtest()
                         .set_color_attachment_format(color_image->getFormat())
                         .set_pipeline_layout(pipeline_layout_);

      blur_x_pipeline_
          = builder.set_shaders(vertex_shader, blur_x_shader).build_pipeline(gpu_.device);
      blur_y_pipeline_
          = builder.set_shaders(vertex_shader, blur_y_shader).build_pipeline(gpu_.device);
    }

    void BloomBlurPass::execute(VkCommandBuffer cmd) {
      const auto image_x = registry_->get_resource<TextureHandle>("scene_brightness_filtered");
      const auto image_y = registry_->get_resource<TextureHandle>("bloom_blur_y");

      for (int i = 0; i < registry_->config_.bloom_quality; i++) {
        {
          blur_x_descriptor_set_ = resource_manager_->descriptor_pool->allocate(
              gpu_.device, descriptor_layouts_.at(0));

          vkutil::Transition(image_x).toLayoutRead().andSubmitTo(cmd);
          vkutil::Transition(image_y).toLayoutWrite().andSubmitTo(cmd);

          VkRenderingAttachmentInfo newColorAttachment
              = vkinit::attachment_info(image_y->imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
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

          vkutil::Transition(image_y).toLayoutRead().andSubmitTo(cmd);
          vkutil::Transition(image_x).toLayoutWrite().andSubmitTo(cmd);

          VkRenderingAttachmentInfo newColorAttachment
              = vkinit::attachment_info(image_x->imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
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

    void BloomBlurPass::cleanup() {
      vkDestroyPipelineLayout(gpu_.device, pipeline_layout_, nullptr);
      vkDestroyPipeline(gpu_.device, blur_x_pipeline_, nullptr);
      vkDestroyPipeline(gpu_.device, blur_y_pipeline_, nullptr);
    }

    void StreaksPass::prepare() {
      fmt::print("Preparing streaks pass\n");

      streak_pattern
          = resource_manager_->load_image("../../assets/StreaksRotationPattern.bmp").value();

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
      VkShaderModule streak_shader;
      vkutil::load_shader_module(fragment_shader_source_.c_str(), gpu_.device, &streak_shader);

      const auto color_image = registry_->get_resource<TextureHandle>("scene_bloom");

      pipeline_ = PipelineBuilder()
                      .set_shaders(vertex_shader, streak_shader)
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

    void StreaksPass::execute(VkCommandBuffer cmd) {
      descriptor_set_
          = resource_manager_->descriptor_pool->allocate(gpu_.device, descriptor_layouts_.at(0));

      const auto scene_bloom = registry_->get_resource<TextureHandle>("scene_bloom");
      const auto color_image
          = registry_->get_resource<TextureHandle>("bloom_blurred_intermediate");

      VkRenderingAttachmentInfo newColorAttachment
          = vkinit::attachment_info(color_image->imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
      VkRenderingInfo newRenderInfo
          = vkinit::rendering_info({effect_size_, effect_size_}, &newColorAttachment, nullptr);
      vkCmdBeginRendering(cmd, &newRenderInfo);

      writer.clear();
      writer.write_image(10, scene_bloom->imageView, repository_->default_material_.nearestSampler,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.write_image(
          11, streak_pattern.imageView, repository_->default_material_.nearestSampler,
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.update_set(gpu_.device, descriptor_set_);

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                              &descriptor_set_, 0, nullptr);
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

      vkCmdSetViewport(cmd, 0, 1, &viewport_);
      vkCmdSetScissor(cmd, 0, 1, &scissor_);

      vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(RenderConfig::StreaksParams), &registry_->config_.streaks);
      vkCmdDraw(cmd, 3, 1, 0, 0);
      vkCmdEndRendering(cmd);
    }

    void StreaksPass::cleanup() {
      vkDestroyPipelineLayout(gpu_.device, pipeline_layout_, nullptr);
      vkDestroyPipeline(gpu_.device, pipeline_, nullptr);
    }

    void LuminancePass::prepare() {
      fmt::print("Preparing luminance pass\n");

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
      VkShaderModule luminance_shader;
      vkutil::load_shader_module(fragment_shader_source_.c_str(), gpu_.device, &luminance_shader);

      const auto color_image = registry_->get_resource<TextureHandle>("lum_64");

      pipeline_ = PipelineBuilder()
                      .set_shaders(vertex_shader, luminance_shader)
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

    void LuminancePass::cleanup() {
      vkDestroyPipelineLayout(gpu_.device, pipeline_layout_, nullptr);
      vkDestroyPipeline(gpu_.device, pipeline_, nullptr);
    }

    void LuminancePass::execute(VkCommandBuffer cmd) {
      descriptor_set_
          = resource_manager_->descriptor_pool->allocate(gpu_.device, descriptor_layouts_.at(0));

      const auto scene_color = registry_->get_resource<TextureHandle>("scene_ssao");
      const auto color_image = registry_->get_resource<TextureHandle>("lum_64");

      vkutil::Transition(scene_color).toLayoutRead().andSubmitTo(cmd);
      vkutil::Transition(color_image).toLayoutWrite().andSubmitTo(cmd);

      VkRenderingAttachmentInfo newColorAttachment
          = vkinit::attachment_info(color_image->imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
      VkRenderingInfo newRenderInfo
          = vkinit::rendering_info(color_image->getExtent2D(), &newColorAttachment, nullptr);
      vkCmdBeginRendering(cmd, &newRenderInfo);

      writer.clear();
      writer.write_image(10, scene_color->imageView, repository_->default_material_.nearestSampler,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.update_set(gpu_.device, descriptor_set_);

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                              &descriptor_set_, 0, nullptr);
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

      viewport_.width = static_cast<float>(color_image->getExtent2D().width);
      viewport_.height = static_cast<float>(color_image->getExtent2D().height);
      scissor_.extent = color_image->getExtent2D();
      vkCmdSetViewport(cmd, 0, 1, &viewport_);
      vkCmdSetScissor(cmd, 0, 1, &scissor_);
      vkCmdDraw(cmd, 3, 1, 0, 0);
      vkCmdEndRendering(cmd);
    }

    void LuminanceDownscalePass::prepare() {
      fmt::print("Preparing luminance downscale pass\n");

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
      VkShaderModule downscale_shader;
      vkutil::load_shader_module(fragment_shader_source_.c_str(), gpu_.device, &downscale_shader);

      const auto color_image = registry_->get_resource<TextureHandle>("lum_64");

      pipeline_ = PipelineBuilder()
                      .set_shaders(vertex_shader, downscale_shader)
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

    void LuminanceDownscalePass::cleanup() {
      vkDestroyPipelineLayout(gpu_.device, pipeline_layout_, nullptr);
      vkDestroyPipeline(gpu_.device, pipeline_, nullptr);
    }

    void LuminanceDownscalePass::execute(VkCommandBuffer cmd) {
      for (size_t i = 0; i < deps_.write_resources.size(); ++i) {
        descriptor_set_
            = resource_manager_->descriptor_pool->allocate(gpu_.device, descriptor_layouts_.at(0));

        std::shared_ptr<TextureHandle> scene_color;
        if (i == 0) {
          scene_color
              = registry_->get_resource<TextureHandle>(deps_.read_resources.at(i)->getId());
        } else {
          scene_color = registry_->get_resource<TextureHandle>(
              deps_.write_resources.at(i - 1).second->getId());
        }

        const auto color_image
            = registry_->get_resource<TextureHandle>(deps_.write_resources.at(i).second->getId());

        vkutil::Transition(scene_color).toLayoutRead().andSubmitTo(cmd);
        vkutil::Transition(color_image).toLayoutWrite().andSubmitTo(cmd);

        VkRenderingAttachmentInfo newColorAttachment
            = vkinit::attachment_info(color_image->imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
        VkRenderingInfo newRenderInfo
            = vkinit::rendering_info(color_image->getExtent2D(), &newColorAttachment, nullptr);
        vkCmdBeginRendering(cmd, &newRenderInfo);

        writer.clear();
        writer.write_image(
            10, scene_color->imageView, repository_->default_material_.nearestSampler,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        writer.update_set(gpu_.device, descriptor_set_);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                                &descriptor_set_, 0, nullptr);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

        viewport_.width = static_cast<float>(color_image->getExtent2D().width);
        viewport_.height = static_cast<float>(color_image->getExtent2D().height);
        scissor_.extent = color_image->getExtent2D();
        vkCmdSetViewport(cmd, 0, 1, &viewport_);
        vkCmdSetScissor(cmd, 0, 1, &scissor_);
        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRendering(cmd);
      }
    }

    void LightAdaptationPass::prepare() {
      fmt::print("Preparing light adaptation pass\n");

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
      VkShaderModule adaptation_shader;
      vkutil::load_shader_module(adaptation_fragment_shader_source_.c_str(), gpu_.device,
                                 &adaptation_shader);

      const auto color_image = registry_->get_resource<TextureHandle>("lum_64");

      pipeline_ = PipelineBuilder()
                      .set_shaders(vertex_shader, adaptation_shader)
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

    void LightAdaptationPass::cleanup() {
      vkDestroyPipelineLayout(gpu_.device, pipeline_layout_, nullptr);
      vkDestroyPipeline(gpu_.device, pipeline_, nullptr);
    }

    void LightAdaptationPass::execute(VkCommandBuffer cmd) {
      descriptor_set_
          = resource_manager_->descriptor_pool->allocate(gpu_.device, descriptor_layouts_.at(0));

      const auto current_lum = registry_->get_resource<TextureHandle>("lum_1_final");
      std::shared_ptr<TextureHandle> avg_lum
          = registry_->get_resource<TextureHandle>("lum_1_avg");
      std::shared_ptr<TextureHandle> new_lum
          = registry_->get_resource<TextureHandle>("lum_1_new");

      vkutil::Transition(current_lum).toLayoutRead().andSubmitTo(cmd);
      vkutil::Transition(avg_lum).toLayoutRead().andSubmitTo(cmd);
      vkutil::Transition(new_lum).toLayoutWrite().andSubmitTo(cmd);

      VkRenderingAttachmentInfo newColorAttachment
          = vkinit::attachment_info(new_lum->imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
      VkRenderingInfo newRenderInfo
          = vkinit::rendering_info(new_lum->getExtent2D(), &newColorAttachment, nullptr);
      vkCmdBeginRendering(cmd, &newRenderInfo);

      writer.clear();
      writer.write_image(10, current_lum->imageView, repository_->default_material_.nearestSampler,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.write_image(11, avg_lum->imageView, repository_->default_material_.nearestSampler,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.update_set(gpu_.device, descriptor_set_);

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                              &descriptor_set_, 0, nullptr);
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

      viewport_.width = static_cast<float>(new_lum->getExtent2D().width);
      viewport_.height = static_cast<float>(new_lum->getExtent2D().height);
      scissor_.extent = new_lum->getExtent2D();
      vkCmdSetViewport(cmd, 0, 1, &viewport_);
      vkCmdSetScissor(cmd, 0, 1, &scissor_);
      vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(RenderConfig::LightAdaptationParams),
                         &registry_->config_.light_adaptation);
      vkCmdDraw(cmd, 3, 1, 0, 0);
      vkCmdEndRendering(cmd);

      // TODO should not be done this way,simulate ping pong buffer
      registry_->update_resource_id("lum_1_avg", "lum_1_temp");
      registry_->update_resource_id("lum_1_new", "lum_1_avg");
      registry_->update_resource_id("lum_1_temp", "lum_1_new");
    }

    void TonemapPass::prepare() {
      fmt::print("Preparing tonemap pass\n");

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
              .build(gpu_.device));
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
      VkShaderModule final_shader;
      vkutil::load_shader_module(fragment_shader_source_.c_str(), gpu_.device, &final_shader);

      const auto color_image = registry_->get_resource<TextureHandle>("hdr_final");

      pipeline_ = PipelineBuilder()
                      .set_shaders(vertex_shader, final_shader)
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

    void TonemapPass::execute(VkCommandBuffer cmd) {
      descriptor_set_
          = resource_manager_->descriptor_pool->allocate(gpu_.device, descriptor_layouts_.at(0));

      const auto scene_color = registry_->get_resource<TextureHandle>("scene_ssao");
      const auto scene_bloom = registry_->get_resource<TextureHandle>("scene_streak");
      const auto color_image = registry_->get_resource<TextureHandle>("hdr_final");
      const auto lum_image = registry_->get_resource<TextureHandle>("lum_1_adapted");
      const auto depth_image = registry_->get_resource<TextureHandle>("directional_shadow_map");

      VkRenderingAttachmentInfo newColorAttachment
          = vkinit::attachment_info(color_image->imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
      VkRenderingInfo newRenderInfo
          = vkinit::rendering_info(color_image->getExtent2D(), &newColorAttachment, nullptr);
      vkCmdBeginRendering(cmd, &newRenderInfo);

      writer.clear();
      writer.write_image(10, scene_color->imageView, repository_->default_material_.nearestSampler,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.write_image(11, scene_bloom->imageView, repository_->default_material_.linearSampler,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.write_image(12, lum_image->imageView, repository_->default_material_.linearSampler,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.update_set(gpu_.device, descriptor_set_);
      writer.write_image(13, depth_image->imageView, repository_->default_material_.linearSampler,
                         VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.update_set(gpu_.device, descriptor_set_);

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                              &descriptor_set_, 0, nullptr);
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

      viewport_.width = static_cast<float>(color_image->getExtent2D().width);
      viewport_.height = static_cast<float>(color_image->getExtent2D().height);
      scissor_.extent = color_image->getExtent2D();
      vkCmdSetViewport(cmd, 0, 1, &viewport_);
      vkCmdSetScissor(cmd, 0, 1, &scissor_);
      vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(RenderConfig::HdrParams), &registry_->config_.hdr);
      vkCmdDraw(cmd, 3, 1, 0, 0);
      vkCmdEndRendering(cmd);
    }

    void TonemapPass::cleanup() {
      vkDestroyPipelineLayout(gpu_.device, pipeline_layout_, nullptr);
      vkDestroyPipeline(gpu_.device, pipeline_, nullptr);
    }
  }  // namespace graphics
}  // namespace gestalt