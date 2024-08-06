#include "RenderPass.hpp"
#include "descriptor.hpp"

#include "vk_images.hpp"
#include "vk_initializers.hpp"
#include "vk_pipelines.hpp"

namespace gestalt::graphics {

  void BrightPass::prepare() {
    fmt::print("Preparing {}\n", get_name());

    descriptor_layouts_.emplace_back(DescriptorLayoutBuilder()
                                         .add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                      VK_SHADER_STAGE_FRAGMENT_BIT)
                                         .build(gpu_->getDevice()));

    dependencies_
        = RenderPassDependencyBuilder()
              .add_shader(ShaderStage::kVertex, "fullscreen.vert.spv")
              .add_shader(ShaderStage::kFragment, "hdr_bright_pass.frag.spv")
              .add_image_attachment(registry_->resources_.scene_color, ImageUsageType::kRead)
              .add_image_attachment(registry_->resources_.bright_pass, ImageUsageType::kWrite)
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
                    .build_graphics_pipeline(gpu_->getDevice());
  }

  void BrightPass::execute(VkCommandBuffer cmd) {
    const auto frame = frame_->get_current_frame_index();

    if (descriptor_buffers_.at(0) == nullptr) {
      for (auto& set : descriptor_buffers_) {
        set = resource_manager_->create_descriptor_buffer(descriptor_layouts_.at(0), 1);

        VkDescriptorImageInfo image_info = {repository_->default_material_.nearestSampler,
                                            registry_->resources_.scene_color.image->imageView,
                                            registry_->resources_.scene_color.image->getLayout()};
        set->
                  write_image(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info)
                  .update();
      }
    }

    begin_renderpass(cmd);

    bind_descriptor_buffers(cmd, {descriptor_buffers_[frame].get()});
    descriptor_buffers_[frame]->bind_descriptors(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdSetViewport(cmd, 0, 1, &viewport_);
    vkCmdSetScissor(cmd, 0, 1, &scissor_);
    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRendering(cmd);
  }

  void BrightPass::destroy() {
    vkDestroyDescriptorSetLayout(gpu_->getDevice(), descriptor_layouts_.at(0), nullptr);

    for (const auto& descriptor_buffer : descriptor_buffers_) {
      resource_manager_->destroy_descriptor_buffer(descriptor_buffer);
    }
  }

  struct BlurDirection {
    int direction;
  };

  void BloomBlurPass::prepare() {
    fmt::print("Preparing {}\n", get_name());

    descriptor_layouts_.emplace_back(DescriptorLayoutBuilder()
                                         .add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                      VK_SHADER_STAGE_FRAGMENT_BIT)
                                         .build(gpu_->getDevice()));

    dependencies_
        = RenderPassDependencyBuilder()
              .add_shader(ShaderStage::kVertex, "fullscreen.vert.spv")
              .add_shader(ShaderStage::kFragment, "hdr_bloom.frag.spv")
              .add_image_attachment(registry_->resources_.bright_pass, ImageUsageType::kCombined)
              .add_image_attachment(registry_->resources_.blur_y, ImageUsageType::kCombined)
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
                    .build_graphics_pipeline(gpu_->getDevice());
  }

  void BloomBlurPass::execute(VkCommandBuffer cmd) {
    const auto frame = frame_->get_current_frame_index();

    const auto image_x = registry_->resources_.bright_pass.image;
    const auto image_y = registry_->resources_.blur_y.image;

    BlurDirection blur_direction{0};

    viewport_.width = static_cast<float>(image_x->getExtent2D().width);
    viewport_.height = static_cast<float>(image_x->getExtent2D().height);
    scissor_.extent = image_x->getExtent2D();

    for (int i = 0; i < registry_->config_.bloom_quality * 2; i++) {
      bool isHorizontal = (i % 2 == 0);
      auto& srcImage = isHorizontal ? image_x : image_y;
      auto& dstImage = isHorizontal ? image_y : image_x;

      vkutil::TransitionImage(srcImage).toLayoutRead().andSubmitTo(cmd);
      vkutil::TransitionImage(dstImage).toLayoutWrite().andSubmitTo(cmd);

      if (descriptor_buffers_[frame][i] == nullptr) {
        descriptor_buffers_[frame][i]
            = resource_manager_->create_descriptor_buffer(descriptor_layouts_.at(0), 1);
        VkDescriptorImageInfo image_info = {repository_->default_material_.nearestSampler,
                                            srcImage->imageView, srcImage->getLayout()};
        descriptor_buffers_[frame][i]
            ->
        write_image(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info)
                  .update();
      }


      VkRenderingAttachmentInfo colorAttachment
          = vkinit::attachment_info(dstImage->imageView, nullptr, dstImage->getLayout());
      VkRenderingInfo renderInfo
          = vkinit::rendering_info(dstImage->getExtent2D(), &colorAttachment, nullptr);
      vkCmdBeginRendering(cmd, &renderInfo);

      
    bind_descriptor_buffers(cmd, {descriptor_buffers_[frame][i].get()});
      descriptor_buffers_[frame][i]->bind_descriptors(
          cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0);
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

  void BloomBlurPass::destroy() {
    vkDestroyDescriptorSetLayout(gpu_->getDevice(), descriptor_layouts_.at(0), nullptr);

    for (const auto& buffers : descriptor_buffers_) {
      for (const auto& buffer : buffers) {
            resource_manager_->destroy_descriptor_buffer(buffer);
        }
    }
  }

  void LuminancePass::prepare() {
    fmt::print("Preparing {}\n", get_name());

    descriptor_layouts_.emplace_back(DescriptorLayoutBuilder()
                                         .add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                      VK_SHADER_STAGE_FRAGMENT_BIT)
                                         .build(gpu_->getDevice()));

    dependencies_
        = RenderPassDependencyBuilder()
              .add_shader(ShaderStage::kVertex, "fullscreen.vert.spv")
              .add_shader(ShaderStage::kFragment, "to_luminance.frag.spv")
              .add_image_attachment(registry_->resources_.scene_color, ImageUsageType::kRead, 2)
              .add_image_attachment(registry_->resources_.lum_64, ImageUsageType::kWrite, 0)
              .build();

    create_pipeline_layout();

    pipeline_ = create_pipeline()
                    .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                    .set_polygon_mode(VK_POLYGON_MODE_FILL)
                    .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                    .set_multisampling_none()
                    .disable_blending()
                    .disable_depthtest()
                    .build_graphics_pipeline(gpu_->getDevice());
  }

  void LuminancePass::execute(VkCommandBuffer cmd) {
    const auto frame = frame_->get_current_frame_index();

    const auto scene_color = registry_->resources_.scene_color.image;

    if (descriptor_buffers_.at(0) == nullptr) {
      for (auto& set : descriptor_buffers_) {
        set = resource_manager_->create_descriptor_buffer(descriptor_layouts_.at(0), 1);
        auto image_info = VkDescriptorImageInfo{repository_->default_material_.nearestSampler,
                                              scene_color->imageView, scene_color->getLayout()};
        set->write_image(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info)
                  .update();
      }
    }

    begin_renderpass(cmd);

    bind_descriptor_buffers(cmd, {descriptor_buffers_.at(frame).get()});
    descriptor_buffers_.at(frame)->bind_descriptors(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    vkCmdSetViewport(cmd, 0, 1, &viewport_);
    vkCmdSetScissor(cmd, 0, 1, &scissor_);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRendering(cmd);
  }

  void LuminancePass::destroy() {
    vkDestroyDescriptorSetLayout(gpu_->getDevice(), descriptor_layouts_.at(0), nullptr);

    for (const auto& buffer : descriptor_buffers_) {
        resource_manager_->destroy_descriptor_buffer(buffer);
    }
  }

  void LuminanceDownscalePass::prepare() {
    fmt::print("Preparing {}\n", get_name());

    descriptor_layouts_.emplace_back(DescriptorLayoutBuilder()
                                         .add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                      VK_SHADER_STAGE_FRAGMENT_BIT)
                                         .build(gpu_->getDevice()));

    dependencies_
        = RenderPassDependencyBuilder()
              .add_shader(ShaderStage::kVertex, "fullscreen.vert.spv")
              .add_shader(ShaderStage::kFragment, "downscale2x2.frag.spv")
              .add_image_attachment(registry_->resources_.lum_64, ImageUsageType::kRead, 1)
              .add_image_attachment(registry_->resources_.lum_32, ImageUsageType::kWrite)
              .add_image_attachment(registry_->resources_.lum_16, ImageUsageType::kCombined)
              .add_image_attachment(registry_->resources_.lum_8, ImageUsageType::kCombined)
              .add_image_attachment(registry_->resources_.lum_4, ImageUsageType::kCombined)
              .add_image_attachment(registry_->resources_.lum_2, ImageUsageType::kCombined)
              .add_image_attachment(registry_->resources_.lum_1, ImageUsageType::kCombined)
              .build();

    create_pipeline_layout();

    pipeline_ = create_pipeline()
                    .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                    .set_polygon_mode(VK_POLYGON_MODE_FILL)
                    .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                    .set_multisampling_none()
                    .disable_blending()
                    .disable_depthtest()
                    .build_graphics_pipeline(gpu_->getDevice());
  }

  void LuminanceDownscalePass::execute(VkCommandBuffer cmd) {
    const auto frame = frame_->get_current_frame_index();

    for (size_t i = 0; i < dependencies_.image_attachments.size() - 1; ++i) {
      const auto src = dependencies_.image_attachments.at(i).attachment.image;
      const auto dst = dependencies_.image_attachments.at(i + 1).attachment.image;

      vkutil::TransitionImage(src).toLayoutRead().andSubmitTo(cmd);
      vkutil::TransitionImage(dst).toLayoutWrite().andSubmitTo(cmd);

      if (descriptor_buffers_[frame].at(i) == nullptr) {
        descriptor_buffers_[frame].at(i)
            = resource_manager_->create_descriptor_buffer(descriptor_layouts_.at(0), 1);
         VkDescriptorImageInfo image_info = {repository_->default_material_.nearestSampler,
                                            src->imageView, src->getLayout()};
        descriptor_buffers_[frame]
            .at(i)
            ->
                         write_image(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info)
                        .update();
      }

      VkRenderingAttachmentInfo newColorAttachment
          = vkinit::attachment_info(dst->imageView, nullptr, dst->getLayout());
      VkRenderingInfo newRenderInfo
          = vkinit::rendering_info(dst->getExtent2D(), &newColorAttachment, nullptr);
      vkCmdBeginRendering(cmd, &newRenderInfo);

      bind_descriptor_buffers(cmd, {descriptor_buffers_[frame].at(i).get()});
      descriptor_buffers_[frame].at(i)->bind_descriptors(
            cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0);
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

  void LuminanceDownscalePass::destroy() {
    vkDestroyDescriptorSetLayout(gpu_->getDevice(), descriptor_layouts_.at(0), nullptr);
  }
  void LightAdaptationPass::prepare() {
    fmt::print("Preparing {}\n", get_name());

    descriptor_layouts_.emplace_back(DescriptorLayoutBuilder()
                                         .add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                      VK_SHADER_STAGE_FRAGMENT_BIT)
                                         .add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                      VK_SHADER_STAGE_FRAGMENT_BIT)
                                         .build(gpu_->getDevice()));

    dependencies_
        = RenderPassDependencyBuilder()
              .add_shader(ShaderStage::kVertex, "fullscreen.vert.spv")
              .add_shader(ShaderStage::kFragment, "hdr_light_adaptation.frag.spv")
              .add_image_attachment(registry_->resources_.lum_1, ImageUsageType::kRead, 1)
              .add_image_attachment(registry_->resources_.lum_A, ImageUsageType::kCombined, 0)
              .add_image_attachment(registry_->resources_.lum_B, ImageUsageType::kCombined, 0)
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
                    .build_graphics_pipeline(gpu_->getDevice());
  }

  void LightAdaptationPass::destroy() {
    vkDestroyDescriptorSetLayout(gpu_->getDevice(), descriptor_layouts_.at(0), nullptr);

    for (const auto& descriptor_buffer : descriptor_buffers_) {
      resource_manager_->destroy_descriptor_buffer(descriptor_buffer);
    }
  }

  void LightAdaptationPass::execute(VkCommandBuffer cmd) {
    const auto frame = frame_->get_current_frame_index();

    const auto current_lum = registry_->resources_.lum_1.image;
    const auto avg_lum
        = frame == 0 ? registry_->resources_.lum_A.image : registry_->resources_.lum_B.image;
    const auto new_lum
        = frame == 0 ? registry_->resources_.lum_B.image : registry_->resources_.lum_A.image;

    vkutil::TransitionImage(current_lum).toLayoutRead().andSubmitTo(cmd);
    vkutil::TransitionImage(avg_lum).toLayoutRead().andSubmitTo(cmd);
    vkutil::TransitionImage(new_lum).toLayoutWrite().andSubmitTo(cmd);

    if (descriptor_buffers_.at(frame) == nullptr) {
      descriptor_buffers_.at(frame)
          = resource_manager_->create_descriptor_buffer(descriptor_layouts_.at(0), 2);

      auto image_info0 = VkDescriptorImageInfo{repository_->default_material_.nearestSampler,
                                               current_lum->imageView, current_lum->getLayout()};
      auto image_info1 = VkDescriptorImageInfo{repository_->default_material_.nearestSampler,
                                               avg_lum->imageView, avg_lum->getLayout()};
      descriptor_buffers_.at(frame)
          ->write_image(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info0)
          .write_image(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info1)
          .update();
    }

    VkRenderingAttachmentInfo newColorAttachment
        = vkinit::attachment_info(new_lum->imageView, nullptr, new_lum->getLayout());
    VkRenderingInfo newRenderInfo
        = vkinit::rendering_info(new_lum->getExtent2D(), &newColorAttachment, nullptr);
    vkCmdBeginRendering(cmd, &newRenderInfo);

    bind_descriptor_buffers(cmd, {descriptor_buffers_.at(frame).get()});
    descriptor_buffers_.at(frame)->bind_descriptors(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0);
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

    descriptor_layouts_.emplace_back(DescriptorLayoutBuilder()
                                         .add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                      VK_SHADER_STAGE_FRAGMENT_BIT)
                                         .add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                      VK_SHADER_STAGE_FRAGMENT_BIT)
                                         .add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                      VK_SHADER_STAGE_FRAGMENT_BIT)
                                         .build(gpu_->getDevice()));

    dependencies_
        = RenderPassDependencyBuilder()
              .add_shader(ShaderStage::kVertex, "fullscreen.vert.spv")
              .add_shader(ShaderStage::kFragment, "hdr_final.frag.spv")
              .add_image_attachment(registry_->resources_.bright_pass, ImageUsageType::kRead, 1)
              .add_image_attachment(registry_->resources_.scene_color, ImageUsageType::kRead, 3)
              .add_image_attachment(registry_->resources_.lum_A, ImageUsageType::kRead, 1)
              .add_image_attachment(registry_->resources_.lum_B, ImageUsageType::kRead, 1)
              .add_image_attachment(registry_->resources_.final_color, ImageUsageType::kWrite, 0)
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
                    .build_graphics_pipeline(gpu_->getDevice());
  }

  void TonemapPass::execute(VkCommandBuffer cmd) {
    const auto frame = frame_->get_current_frame_index();

    const auto scene_linear = registry_->resources_.scene_color.image;
    const auto scene_bloom = registry_->resources_.bright_pass.image;
    const auto lum_image
        = frame == 0 ? registry_->resources_.lum_B.image : registry_->resources_.lum_A.image;

    if (descriptor_buffers_.at(0) == nullptr) {
      for (auto& set : descriptor_buffers_) {
        set = resource_manager_->create_descriptor_buffer(descriptor_layouts_.at(0), 3);
        auto image_info0 = VkDescriptorImageInfo{repository_->default_material_.nearestSampler,
                                              scene_linear->imageView, scene_linear->getLayout()};
         auto image_info1 = VkDescriptorImageInfo{repository_->default_material_.linearSampler,
                                                    scene_bloom->imageView, scene_bloom->getLayout()};
         auto image_info2 = VkDescriptorImageInfo{repository_->default_material_.linearSampler,
                                                    lum_image->imageView, lum_image->getLayout()};
         set->
                         write_image(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info0)
                        .write_image(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info1)
                        .write_image(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info2)
                        .update();
      }
    }

    begin_renderpass(cmd);

    bind_descriptor_buffers(cmd, {descriptor_buffers_.at(frame).get()});
    descriptor_buffers_.at(frame)->bind_descriptors(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdSetViewport(cmd, 0, 1, &viewport_);
    vkCmdSetScissor(cmd, 0, 1, &scissor_);
    vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(RenderConfig::HdrParams), &registry_->config_.hdr);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRendering(cmd);
  }

  void TonemapPass::destroy() {
    vkDestroyDescriptorSetLayout(gpu_->getDevice(), descriptor_layouts_.at(0), nullptr);

    for (const auto& descriptor_buffer : descriptor_buffers_) {
      resource_manager_->destroy_descriptor_buffer(descriptor_buffer);
    }
  }
}  // namespace gestalt::graphics