#include "HdrPass.h"

#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"

namespace gestalt {
  namespace graphics {
    using namespace foundation;
    void BrightPass::prepare() {
      fmt::print("Preparing {}\n", get_name());

      descriptor_layouts_.emplace_back(DescriptorLayoutBuilder()
                                           .add_binding(10,
                                                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT)
                                           .build(gpu_.device));

      dependencies_
          = RenderPassDependencyBuilder()
                .add_shader(ShaderStage::kVertex, "fullscreen.vert.spv")
                .add_shader(ShaderStage::kFragment, "hdr_bright_pass.frag.spv")
                .add_image_attachment(registry_->attachments_.scene_color, ImageUsageType::kRead)
                .add_image_attachment(registry_->attachments_.bright_pass, ImageUsageType::kWrite)
                .set_push_constant_range(sizeof(RenderConfig::HdrParams),
                                         VK_SHADER_STAGE_FRAGMENT_BIT)
                .build();

      create_pipeline_layout();

      pipeline_ = create_pipeline()
                      .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                      .set_polygon_mode(VK_POLYGON_MODE_FILL)
                      .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                      .set_multisampling_none()
                      .disable_blending()
                      .disable_depthtest()
                      .build_pipeline(gpu_.device);
    }

    void BrightPass::execute(VkCommandBuffer cmd) {
      descriptor_set_
          = resource_manager_->descriptor_pool->allocate(gpu_.device, descriptor_layouts_.at(0));

      const auto scene_ssao = registry_->attachments_.scene_color.image;

      begin_renderpass(cmd);

      writer.clear();
      writer.write_image(10, scene_ssao->imageView, repository_->default_material_.nearestSampler,
                         scene_ssao->getLayout(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.update_set(gpu_.device, descriptor_set_);

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                              &descriptor_set_, 0, nullptr);
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
      vkCmdSetViewport(cmd, 0, 1, &viewport_);
      vkCmdSetScissor(cmd, 0, 1, &scissor_);
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRendering(cmd);
    }

    void BrightPass::destroy() {}
    struct BlurDirection {
      int direction;
    };

    void BloomBlurPass::prepare() {
      fmt::print("Preparing {}\n", get_name());

      descriptor_layouts_.emplace_back(DescriptorLayoutBuilder()
                                           .add_binding(10,
                                                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT)
                                           .build(gpu_.device));

      dependencies_
          = RenderPassDependencyBuilder()
                .add_shader(ShaderStage::kVertex, "fullscreen.vert.spv")
                .add_shader(ShaderStage::kFragment, "hdr_bloom.frag.spv")
                .add_image_attachment(registry_->attachments_.bright_pass, ImageUsageType::kRead)
                .add_image_attachment(registry_->attachments_.scene_bloom, ImageUsageType::kWrite)
                .set_push_constant_range(sizeof(BlurDirection), VK_SHADER_STAGE_FRAGMENT_BIT)
                .build();

      create_pipeline_layout();

      pipeline_ = create_pipeline()
                      .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                      .set_polygon_mode(VK_POLYGON_MODE_FILL)
                      .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                      .set_multisampling_none()
                      .disable_blending()
                      .disable_depthtest()
                      .build_pipeline(gpu_.device);
    }

    void BloomBlurPass::execute(VkCommandBuffer cmd) {
      const auto image_x = registry_->attachments_.bright_pass.image;
      const auto image_y = registry_->attachments_.scene_bloom.image;

      BlurDirection blur_direction{0};

      viewport_.width = static_cast<float>(image_x->getExtent2D().width);
      viewport_.height = static_cast<float>(image_x->getExtent2D().height);
      scissor_.extent = image_x->getExtent2D();

      for (int i = 0; i < registry_->config_.bloom_quality * 2; i++) {
          descriptor_set_ = resource_manager_->descriptor_pool->allocate(gpu_.device, descriptor_layouts_.at(0));

        bool isHorizontal = (i % 2 == 0);
        auto& srcImage = isHorizontal ? image_x : image_y;
        auto& dstImage = isHorizontal ? image_y : image_x;

        vkutil::TransitionImage(srcImage).toLayoutRead().andSubmitTo(cmd);
        vkutil::TransitionImage(dstImage).toLayoutWrite().andSubmitTo(cmd);

        VkRenderingAttachmentInfo colorAttachment
            = vkinit::attachment_info(dstImage->imageView, nullptr, dstImage->getLayout());
        VkRenderingInfo renderInfo
            = vkinit::rendering_info(dstImage->getExtent2D(), &colorAttachment, nullptr);
        vkCmdBeginRendering(cmd, &renderInfo);

        writer.clear();
        writer.write_image(10, srcImage->imageView, repository_->default_material_.nearestSampler,
                           srcImage->getLayout(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        writer.update_set(gpu_.device, descriptor_set_);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                                &descriptor_set_, 0, nullptr);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

        blur_direction.direction = i % 2;
        vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(BlurDirection), &blur_direction);
        vkCmdSetViewport(cmd, 0, 1, &viewport_);
        vkCmdSetScissor(cmd, 0, 1, &scissor_);
        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRendering(cmd);
      }
    }

    void BloomBlurPass::destroy() {}

    void LuminancePass::prepare() {
      fmt::print("Preparing {}\n", get_name());

      descriptor_layouts_.emplace_back(DescriptorLayoutBuilder()
                                           .add_binding(10,
                                                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT)
                                           .build(gpu_.device));

      dependencies_
          = RenderPassDependencyBuilder()
                .add_shader(ShaderStage::kVertex, "fullscreen.vert.spv")
                .add_shader(ShaderStage::kFragment, "to_luminance.frag.spv")
                .add_image_attachment(registry_->attachments_.scene_color, ImageUsageType::kRead)
                .add_image_attachment(registry_->attachments_.lum_64, ImageUsageType::kWrite)
                .build();

      create_pipeline_layout();

      pipeline_ = create_pipeline()
                      .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                      .set_polygon_mode(VK_POLYGON_MODE_FILL)
                      .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                      .set_multisampling_none()
                      .disable_blending()
                      .disable_depthtest()
                      .build_pipeline(gpu_.device);
    }

    void LuminancePass::execute(VkCommandBuffer cmd) {
      descriptor_set_
          = resource_manager_->descriptor_pool->allocate(gpu_.device, descriptor_layouts_.at(0));

      const auto scene_color = registry_->attachments_.scene_color.image;

      begin_renderpass(cmd);

      writer.clear();
      writer.write_image(10, scene_color->imageView, repository_->default_material_.nearestSampler,
                         scene_color->getLayout(),
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

    void LuminancePass::destroy() {}

    void LuminanceDownscalePass::prepare() {
      fmt::print("Preparing {}\n", get_name());

      descriptor_layouts_.emplace_back(DescriptorLayoutBuilder()
                                           .add_binding(10,
                                                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT)
                                           .build(gpu_.device));
      dependencies_
          = RenderPassDependencyBuilder()
                .add_shader(ShaderStage::kVertex, "fullscreen.vert.spv")
                .add_shader(ShaderStage::kFragment, "downscale2x2.frag.spv")
                .add_image_attachment(registry_->attachments_.lum_64, ImageUsageType::kRead)
                .add_image_attachment(registry_->attachments_.lum_32, ImageUsageType::kWrite)
                .add_image_attachment(registry_->attachments_.lum_16, ImageUsageType::kCombined)
                .add_image_attachment(registry_->attachments_.lum_8, ImageUsageType::kCombined)
                .add_image_attachment(registry_->attachments_.lum_4, ImageUsageType::kCombined)
                .add_image_attachment(registry_->attachments_.lum_2, ImageUsageType::kCombined)
                .add_image_attachment(registry_->attachments_.lum_1, ImageUsageType::kCombined)
                .build();

      create_pipeline_layout();

      pipeline_ = create_pipeline()
                      .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                      .set_polygon_mode(VK_POLYGON_MODE_FILL)
                      .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                      .set_multisampling_none()
                      .disable_blending()
                      .disable_depthtest()
                      .build_pipeline(gpu_.device);
    }

    void LuminanceDownscalePass::execute(VkCommandBuffer cmd) {
      
      for (size_t i = 0; i < dependencies_.image_attachments.size() - 1; ++i) {
        descriptor_set_
            = resource_manager_->descriptor_pool->allocate(gpu_.device, descriptor_layouts_.at(0));

        const auto src = dependencies_.image_attachments.at(i).attachment.image;
        const auto dst = dependencies_.image_attachments.at(i + 1).attachment.image;

        vkutil::TransitionImage(src).toLayoutRead().andSubmitTo(cmd);
        vkutil::TransitionImage(dst).toLayoutWrite().andSubmitTo(cmd);

        VkRenderingAttachmentInfo newColorAttachment
            = vkinit::attachment_info(dst->imageView, nullptr, dst->getLayout());
        VkRenderingInfo newRenderInfo
            = vkinit::rendering_info(dst->getExtent2D(), &newColorAttachment, nullptr);
        vkCmdBeginRendering(cmd, &newRenderInfo);

        writer.clear();
        writer.write_image(10, src->imageView, repository_->default_material_.nearestSampler,
            src->getLayout(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        writer.update_set(gpu_.device, descriptor_set_);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                                &descriptor_set_, 0, nullptr);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

        viewport_.width = static_cast<float>(dst->getExtent2D().width);
        viewport_.height = static_cast<float>(dst->getExtent2D().height);
        scissor_.extent = dst->getExtent2D();
        vkCmdSetViewport(cmd, 0, 1, &viewport_);
        vkCmdSetScissor(cmd, 0, 1, &scissor_);
        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRendering(cmd);
      }
    }

    void LuminanceDownscalePass::destroy() {}
    void LightAdaptationPass::prepare() {
      fmt::print("Preparing {}\n", get_name());

      descriptor_layouts_.emplace_back(
          DescriptorLayoutBuilder()
              .add_binding(10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_FRAGMENT_BIT)
              .add_binding(11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_FRAGMENT_BIT)
              .build(gpu_.device));

      dependencies_
          = RenderPassDependencyBuilder()
                .add_shader(ShaderStage::kVertex, "fullscreen.vert.spv")
                .add_shader(ShaderStage::kFragment, "hdr_light_adaptation.frag.spv")
                .add_image_attachment(registry_->attachments_.lum_1, ImageUsageType::kRead)
                .add_image_attachment(registry_->attachments_.lum_A, ImageUsageType::kRead)
                .add_image_attachment(registry_->attachments_.lum_B, ImageUsageType::kWrite)
                .set_push_constant_range(sizeof(RenderConfig::LightAdaptationParams),
                                         VK_SHADER_STAGE_FRAGMENT_BIT)
                .build();

      create_pipeline_layout();

      pipeline_ = create_pipeline()
                      .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                      .set_polygon_mode(VK_POLYGON_MODE_FILL)
                      .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                      .set_multisampling_none()
                      .disable_blending()
                      .disable_depthtest()
                      .build_pipeline(gpu_.device);
    }

    void LightAdaptationPass::destroy() {}

    void LightAdaptationPass::execute(VkCommandBuffer cmd) {
      descriptor_set_
          = resource_manager_->descriptor_pool->allocate(gpu_.device, descriptor_layouts_.at(0));

      char currentFrame = gpu_.get_current_frame();

      const auto current_lum = registry_->attachments_.lum_1.image;
      const auto avg_lum = currentFrame == 0 ? registry_->attachments_.lum_A.image : registry_->attachments_.lum_B.image;
      const auto new_lum = currentFrame == 0 ? registry_->attachments_.lum_B.image : registry_->attachments_.lum_A.image;

      vkutil::TransitionImage(current_lum).toLayoutRead().andSubmitTo(cmd);
      vkutil::TransitionImage(avg_lum).toLayoutRead().andSubmitTo(cmd);
      vkutil::TransitionImage(new_lum).toLayoutWrite().andSubmitTo(cmd);

      VkRenderingAttachmentInfo newColorAttachment = vkinit::attachment_info(
          new_lum->imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      VkRenderingInfo newRenderInfo
          = vkinit::rendering_info(new_lum->getExtent2D(), &newColorAttachment, nullptr);
      vkCmdBeginRendering(cmd, &newRenderInfo);

      writer.clear();
      writer.write_image(10, current_lum->imageView, repository_->default_material_.nearestSampler,
                         current_lum->getLayout(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.write_image(11, avg_lum->imageView, repository_->default_material_.nearestSampler,
                         avg_lum->getLayout(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
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
    }

    void TonemapPass::prepare() {
      fmt::print("Preparing {}\n", get_name());

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

      dependencies_
          = RenderPassDependencyBuilder()
                .add_shader(ShaderStage::kVertex, "fullscreen.vert.spv")
                .add_shader(ShaderStage::kFragment, "hdr_final.frag.spv")
                .add_image_attachment(registry_->attachments_.bright_pass, ImageUsageType::kRead)
                .add_image_attachment(registry_->attachments_.scene_color, ImageUsageType::kRead)
                .add_image_attachment(registry_->attachments_.lum_A, ImageUsageType::kRead)
                .add_image_attachment(registry_->attachments_.lum_B, ImageUsageType::kRead)
                .add_image_attachment(registry_->attachments_.final_color, ImageUsageType::kWrite)
                .set_push_constant_range(sizeof(RenderConfig::HdrParams),
                                         VK_SHADER_STAGE_FRAGMENT_BIT)
                .build();

      create_pipeline_layout();

      pipeline_ = create_pipeline()
                      .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                      .set_polygon_mode(VK_POLYGON_MODE_FILL)
                      .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                      .set_multisampling_none()
                      .disable_blending()
                      .disable_depthtest()
                      .build_pipeline(gpu_.device);
    }

    void TonemapPass::execute(VkCommandBuffer cmd) {
      descriptor_set_
          = resource_manager_->descriptor_pool->allocate(gpu_.device, descriptor_layouts_.at(0));
      char currentFrame = gpu_.get_current_frame();
      const auto scene_linear = registry_->attachments_.scene_color.image;
      const auto scene_bloom = registry_->attachments_.bright_pass.image;
      const auto lum_image = currentFrame == 0 ? registry_->attachments_.lum_B.image
                                            : registry_->attachments_.lum_A.image;
      begin_renderpass(cmd);

      writer.clear();
      writer.write_image(10, scene_linear->imageView, repository_->default_material_.nearestSampler,
                         scene_linear->getLayout(),
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.write_image(11, scene_bloom->imageView, repository_->default_material_.linearSampler,
                         scene_bloom->getLayout(),
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.write_image(12, lum_image->imageView, repository_->default_material_.linearSampler,
                         lum_image->getLayout(),
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.update_set(gpu_.device, descriptor_set_);

      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                              &descriptor_set_, 0, nullptr);
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
      vkCmdSetViewport(cmd, 0, 1, &viewport_);
      vkCmdSetScissor(cmd, 0, 1, &scissor_);
      vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(RenderConfig::HdrParams), &registry_->config_.hdr);
      vkCmdDraw(cmd, 3, 1, 0, 0);
      vkCmdEndRendering(cmd);
    }

    void TonemapPass::destroy() {}
  }  // namespace graphics
}  // namespace gestalt