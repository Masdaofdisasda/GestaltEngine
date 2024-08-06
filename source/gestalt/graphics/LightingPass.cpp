#include "RenderPass.hpp"
#include "descriptor.hpp"

#include "vk_images.hpp"
#include "vk_initializers.hpp"
#include "vk_pipelines.hpp"


namespace gestalt::graphics {

  void LightingPass::prepare() {
    fmt::print("Preparing {}\n", get_name());

    descriptor_layouts_.emplace_back(
        DescriptorLayoutBuilder()
            .add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT
                             | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_COMPUTE_BIT
                             | VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(gpu_->getDevice()));
    descriptor_layouts_.push_back(
        DescriptorLayoutBuilder()
            .add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT,
                         false, getMaxTextures())
            .add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(gpu_->getDevice()));
    descriptor_layouts_.emplace_back(
        DescriptorLayoutBuilder()
            .add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                         VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, false,
                         getMaxLights())
            .add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, false,
                         getMaxDirectionalLights())
            .add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, false,
                         getMaxPointLights())
            .build(gpu_->getDevice()));
    descriptor_layouts_.emplace_back(
        DescriptorLayoutBuilder()
            .add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_binding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(gpu_->getDevice(), VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT));

    dependencies_
        = RenderPassDependencyBuilder()
              .add_shader(ShaderStage::kVertex, "fullscreen.vert.spv")
              .add_shader(ShaderStage::kFragment, "pbr_lighting.frag.spv")
              .add_image_attachment(registry_->resources_.gbuffer1, ImageUsageType::kRead, 1)
              .add_image_attachment(registry_->resources_.gbuffer2, ImageUsageType::kRead, 1)
              .add_image_attachment(registry_->resources_.gbuffer3, ImageUsageType::kRead, 1)
              .add_image_attachment(registry_->resources_.shadow_map, ImageUsageType::kRead, 1)
              .add_image_attachment(registry_->resources_.scene_color, ImageUsageType::kWrite, 0)
              .add_image_attachment(registry_->resources_.scene_depth, ImageUsageType::kRead, 1)
              .set_push_constant_range(sizeof(RenderConfig::LightingParams),
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

  void LightingPass::destroy() {
    vkDestroyDescriptorSetLayout(gpu_->getDevice(), descriptor_layouts_.at(1), nullptr);

    for (const auto& descriptor_buffer : descriptor_buffers_) {
      resource_manager_->destroy_descriptor_buffer(descriptor_buffer);
    }
  }

  void LightingPass::execute(VkCommandBuffer cmd) {
    begin_renderpass(cmd);

    const auto frame = current_frame_index;
    const auto& per_frame_buffers = repository_->per_frame_data_buffers;
    const auto& material_buffers = repository_->material_buffers;
    const auto& light_data = repository_->light_buffers;

    const auto gbuffer_1 = registry_->resources_.gbuffer1.image;
    const auto gbuffer_2 = registry_->resources_.gbuffer2.image;
    const auto gbuffer_3 = registry_->resources_.gbuffer3.image;
    const auto gbuffer_depth = registry_->resources_.scene_depth.image;
    const auto shadow_map = registry_->resources_.shadow_map.image;

    if (descriptor_buffers_.at(frame) == nullptr) {
      descriptor_buffers_.at(frame)
          = resource_manager_->create_descriptor_buffer(descriptor_layouts_.at(1), 5, 0);
      auto image_info0 = VkDescriptorImageInfo{repository_->default_material_.nearestSampler,
                                               gbuffer_1->imageView, gbuffer_1->getLayout()};
      auto image_info1 = VkDescriptorImageInfo{repository_->default_material_.nearestSampler,
                                               gbuffer_2->imageView, gbuffer_2->getLayout()};
      auto image_info2 = VkDescriptorImageInfo{repository_->default_material_.nearestSampler,
                                               gbuffer_3->imageView, gbuffer_3->getLayout()};
      auto image_info3
          = VkDescriptorImageInfo{repository_->default_material_.nearestSampler,
                                  gbuffer_depth->imageView, gbuffer_depth->getLayout()};
      auto image_info4 = VkDescriptorImageInfo{repository_->default_material_.nearestSampler,
                                               shadow_map->imageView, shadow_map->getLayout()};
      descriptor_buffers_.at(frame)
          ->write_image(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info0)
          .write_image(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info1)
          .write_image(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info2)
          .write_image(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info3)
          .write_image(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info4)
          .update();
    }

    bind_descriptor_buffers(
        cmd, {per_frame_buffers->descriptor_buffers[frame].get(),
              material_buffers->descriptor_buffer.get(), light_data->descriptor_buffer.get(),
              descriptor_buffers_.at(frame).get()});
    per_frame_buffers->descriptor_buffers[frame]->bind_descriptors(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0);
    material_buffers->descriptor_buffer->bind_descriptors(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                          pipeline_layout_, 1);
    light_data->descriptor_buffer->bind_descriptors(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                    pipeline_layout_, 2);
    descriptor_buffers_.at(frame)->bind_descriptors(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                    pipeline_layout_, 3);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdSetViewport(cmd, 0, 1, &viewport_);
    vkCmdSetScissor(cmd, 0, 1, &scissor_);
    registry_->config_.lighting.invViewProj = per_frame_buffers->data.inv_viewProj;
    registry_->config_.lighting.num_dir_lights = repository_->directional_lights.size();
    registry_->config_.lighting.num_point_lights = repository_->point_lights.size();

    vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(RenderConfig::LightingParams), &registry_->config_.lighting);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRendering(cmd);
  }
}  // namespace gestalt::graphics