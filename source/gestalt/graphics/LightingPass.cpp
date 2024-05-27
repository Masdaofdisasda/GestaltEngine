#include "RenderPass.hpp"

#include "vk_images.hpp"
#include "vk_initializers.hpp"
#include "vk_pipelines.hpp"


namespace gestalt {
  namespace graphics {
    using namespace foundation;
    void LightingPass::prepare() {
      fmt::print("preparing lighting pass\n");

      const auto& per_frame_buffers = repository_->get_buffer<PerFrameDataBuffers>();
      const auto& light_data = repository_->get_buffer<LightBuffers>();
      const auto& ibl_buffers = repository_->get_buffer<IblBuffers>();
      descriptor_layouts_.push_back(per_frame_buffers.descriptor_layout);
      descriptor_layouts_.push_back(ibl_buffers.descriptor_layout);
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
      descriptor_layouts_.push_back(light_data.descriptor_layout);

      dependencies_
          = RenderPassDependencyBuilder()
                .add_shader(ShaderStage::kVertex, "fullscreen.vert.spv")
                .add_shader(ShaderStage::kFragment, "pbr_lighting.frag.spv")
                .add_image_attachment(registry_->attachments_.gbuffer1, ImageUsageType::kRead, 1)
                .add_image_attachment(registry_->attachments_.gbuffer2, ImageUsageType::kRead, 1)
                .add_image_attachment(registry_->attachments_.gbuffer3, ImageUsageType::kRead, 1)
                .add_image_attachment(registry_->attachments_.shadow_map, ImageUsageType::kRead, 1)
                .add_image_attachment(registry_->attachments_.scene_color, ImageUsageType::kWrite, 0)
                .add_image_attachment(registry_->attachments_.scene_depth, ImageUsageType::kRead,
                                      1)
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
                      .build_pipeline(gpu_.device);
    }

    void LightingPass::destroy() {  }

    void LightingPass::execute(VkCommandBuffer cmd) {
      descriptor_set_
          = resource_manager_->descriptor_pool->allocate(gpu_.device, descriptor_layouts_.at(2));

      begin_renderpass(cmd);

      const auto frame = gpu_frame.get_current_frame();
      const auto& per_frame_buffers = repository_->get_buffer<PerFrameDataBuffers>();
      const auto& ibl_buffers = repository_->get_buffer<IblBuffers>();
      auto& light_data = repository_->get_buffer<LightBuffers>();
      const auto& mesh_buffers = repository_->get_buffer<MeshBuffers>();

      vkCmdBindIndexBuffer(cmd, mesh_buffers.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

      const auto gbuffer_1 = registry_->attachments_.gbuffer1.image;
      const auto gbuffer_2 = registry_->attachments_.gbuffer2.image;
      const auto gbuffer_3 = registry_->attachments_.gbuffer3.image;
      const auto gbuffer_depth = registry_->attachments_.scene_depth.image;
      const auto shadow_map = registry_->attachments_.shadow_map.image;

      writer_.clear();
      writer_.write_image(10, gbuffer_1->imageView, repository_->default_material_.nearestSampler,
                          gbuffer_1->getLayout(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer_.write_image(11, gbuffer_2->imageView, repository_->default_material_.nearestSampler,
                          gbuffer_2->getLayout(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer_.write_image(12, gbuffer_3->imageView, repository_->default_material_.nearestSampler,
                          gbuffer_3->getLayout(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer_.write_image(13, gbuffer_depth->imageView,
                          repository_->default_material_.nearestSampler, gbuffer_depth->getLayout(),
                          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer_.write_image(14, shadow_map->imageView, repository_->default_material_.nearestSampler,
                          shadow_map->getLayout(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer_.update_set(gpu_.device, descriptor_set_);

      VkDescriptorSet descriptorSets[]
          = {per_frame_buffers.descriptor_sets[frame], ibl_buffers.descriptor_set,
             descriptor_set_, light_data.descriptor_set};
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 4,
                              descriptorSets, 0, nullptr);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
      vkCmdSetViewport(cmd, 0, 1, &viewport_);
      vkCmdSetScissor(cmd, 0, 1, &scissor_);
      registry_->config_.lighting.invViewProj = per_frame_buffers.data.inv_viewproj;
      registry_->config_.lighting.num_dir_lights = repository_->directional_lights.size();
      registry_->config_.lighting.num_point_lights = repository_->point_lights.size();

      vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(RenderConfig::LightingParams), &registry_->config_.lighting);
      vkCmdDraw(cmd, 3, 1, 0, 0);
      vkCmdEndRendering(cmd);
    }
  }  // namespace graphics
}  // namespace gestalt