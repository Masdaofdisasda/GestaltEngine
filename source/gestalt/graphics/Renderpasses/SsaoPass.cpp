#include "Renderpasses/RenderPassTypes.hpp"

#include <fmt/core.h>

#include "FrameProvider.hpp"
#include "ResourceManager.hpp"
#include "Interface/IGpu.hpp"
#include "Render Engine/ResourceRegistry.hpp"
#include "Utils/vk_descriptors.hpp"


namespace gestalt::graphics {

  void SsaoPass::prepare() {
    fmt::print("Preparing {}\n", get_name());

    rotation_texture_ = resource_manager_->load_image("../../assets/rot_texture.bmp").value();

    descriptor_layouts_.emplace_back(
        DescriptorLayoutBuilder()
            .add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT )
            .build(gpu_->getDevice()));
    descriptor_layouts_.emplace_back(
        DescriptorLayoutBuilder()
            .add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(gpu_->getDevice()));

    dependencies_ = RenderPassDependencyBuilder()
                        .add_shader(ShaderStage::kCompute, "ssao_filter.comp.spv")
                        .add_image_attachment(registry_->resources_.scene_depth,
                                              ImageUsageType::kRead)
                        .add_image_attachment(registry_->resources_.gbuffer2,
                                              ImageUsageType::kRead)
                        .add_image_attachment(registry_->resources_.ambient_occlusion,
                                              ImageUsageType::kStore)
              .set_push_constant_range(sizeof(RenderConfig::SsaoParams),
                                                 VK_SHADER_STAGE_COMPUTE_BIT)
                        .build();

    create_pipeline_layout();

    pipeline_ = create_pipeline().build_compute_pipeline(gpu_->getDevice());
  }

  void SsaoPass::destroy() {
    vkDestroyDescriptorSetLayout(gpu_->getDevice(), descriptor_layouts_.at(1), nullptr);

    for (const auto& descriptor_buffer : descriptor_buffers_) {
      resource_manager_->destroy_descriptor_buffer(descriptor_buffer);
    }
  }

  void SsaoPass::execute(VkCommandBuffer cmd) {
    begin_compute_pass(cmd);
    const auto frame = frame_->get_current_frame_index();


    const auto& per_frame_buffers = repository_->per_frame_data_buffers;

    const auto gbuffer_depth = registry_->resources_.scene_depth.image;
    const auto gbuffer_2 = registry_->resources_.gbuffer2.image;
    const auto occlusion = registry_->resources_.ambient_occlusion.image;

    if (descriptor_buffers_.at(frame) == nullptr) {
      descriptor_buffers_.at(frame)
          = resource_manager_->create_descriptor_buffer(descriptor_layouts_.at(1), 4, 0);
      auto image_info0 = VkDescriptorImageInfo{post_process_sampler, gbuffer_depth->imageView,
                                               gbuffer_depth->getLayout()};
      auto image_info1 = VkDescriptorImageInfo{post_process_sampler, gbuffer_2->imageView,
                                               gbuffer_2->getLayout()};
      auto image_info2 = VkDescriptorImageInfo{post_process_sampler, rotation_texture_.imageView,
                                               rotation_texture_.getLayout()};
      auto image_info3 = VkDescriptorImageInfo{post_process_sampler, occlusion->imageView,
                                               occlusion->getLayout()};
      descriptor_buffers_.at(frame)
          ->write_image(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info0)
          .write_image(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info1)
          .write_image(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info2)
          .write_image(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, image_info3)
          .update();
    }

    bind_descriptor_buffers(cmd, {per_frame_buffers->descriptor_buffers[frame].get(), descriptor_buffers_.at(frame).get()});
    per_frame_buffers->descriptor_buffers[frame]->bind_descriptors(
        cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout_, 0);
    descriptor_buffers_.at(frame)->bind_descriptors(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                                    pipeline_layout_, 1);

    vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(RenderConfig::SsaoParams), &registry_->config_.ssao);
    vkCmdDispatch(cmd, occlusion->getExtent2D().width / 8, occlusion->getExtent2D().height / 8, 1);
  }
  
  struct BlurPushConstants {
    float32 blurRadius;
    float32 depthThreshold;
  };

  void SsaoBlurPass::prepare() {
    fmt::print("Preparing {}\n", get_name());

    descriptor_layouts_.emplace_back(
        DescriptorLayoutBuilder()
            .add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
            .add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
            .build(gpu_->getDevice()));

    dependencies_
        = RenderPassDependencyBuilder()
              .add_shader(ShaderStage::kCompute, "bilateral_blur.comp.spv")
              .add_image_attachment(registry_->resources_.ambient_occlusion, ImageUsageType::kRead)
              .add_image_attachment(registry_->resources_.scene_depth, ImageUsageType::kRead)
              .add_image_attachment(registry_->resources_.ambient_occlusion_blurred, ImageUsageType::kStore)
              .set_push_constant_range(sizeof(BlurPushConstants), VK_SHADER_STAGE_COMPUTE_BIT)
              .build();

    create_pipeline_layout();

    pipeline_ = create_pipeline().build_compute_pipeline(gpu_->getDevice());
  }

  void SsaoBlurPass::execute(VkCommandBuffer cmd) {
    const auto frame = frame_->get_current_frame_index();

    const auto ambient_occlusion = registry_->resources_.ambient_occlusion.image;
    const auto scene_depth = registry_->resources_.scene_depth.image;
    const auto ambient_occlusion_blurred = registry_->resources_.ambient_occlusion_blurred.image;

    if (descriptor_buffers_.at(frame) == nullptr) {
      descriptor_buffers_.at(frame)
          = resource_manager_->create_descriptor_buffer(descriptor_layouts_.at(0), 3, 0);
      auto image_info0 = VkDescriptorImageInfo{post_process_sampler, ambient_occlusion->imageView,
                                               ambient_occlusion->getLayout()};
      auto image_info1 = VkDescriptorImageInfo{post_process_sampler, scene_depth->imageView,
                                               scene_depth->getLayout()};
      auto image_info2 = VkDescriptorImageInfo{
          post_process_sampler, ambient_occlusion_blurred->imageView,
                                  ambient_occlusion_blurred->getLayout()};
      descriptor_buffers_.at(frame)
          ->write_image(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info0)
          .write_image(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info1)
          .write_image(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, image_info2)
          .update();
    }

    bind_descriptor_buffers(cmd, {descriptor_buffers_.at(frame).get()});
    descriptor_buffers_.at(frame)->bind_descriptors(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                                    pipeline_layout_, 0);

    const BlurPushConstants constants
        = {registry_->config_.ssao_radius, registry_->config_.depthThreshold};

    begin_compute_pass(cmd);
    vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(BlurPushConstants), &constants);
    vkCmdDispatch(cmd, ambient_occlusion_blurred->getExtent2D().width / 8,
                  ambient_occlusion_blurred->getExtent2D().height / 8, 1);
  }

  void SsaoBlurPass::destroy() {
    vkDestroyDescriptorSetLayout(gpu_->getDevice(), descriptor_layouts_.at(1), nullptr);

    for (const auto& descriptor_buffer : descriptor_buffers_) {
      resource_manager_->destroy_descriptor_buffer(descriptor_buffer);
    }
  }


}  // namespace gestalt::graphics