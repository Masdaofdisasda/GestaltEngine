#include "hdr_pass.h"

#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"
#include "vk_renderer.h"


void hdr_pass::prepare() {
  resource_manager_->create_framebuffer({effect_size_, effect_size_, 1}, hdr_buffer_);
  streak_pattern = resource_manager_->load_image("../../assets/StreaksRotationPattern.bmp").value();

  VkShaderModule vertex_shader;
  vkutil::load_shader_module(vertex_hdr.c_str(), gpu_.device, &vertex_shader);
  VkShaderModule bright_pass_shader;
  vkutil::load_shader_module(bright_pass_.fragment.c_str(), gpu_.device, &bright_pass_shader);
  VkShaderModule adaptation_shader;
  vkutil::load_shader_module(adaptation_.fragment.c_str(), gpu_.device, &adaptation_shader);
  VkShaderModule streaks_shader;
  vkutil::load_shader_module(streaks_.fragment.c_str(), gpu_.device, &streaks_shader);
  VkShaderModule blur_x_shader;
  vkutil::load_shader_module(blur_x_.fragment.c_str(), gpu_.device, &blur_x_shader);
  VkShaderModule blur_y_shader;
  vkutil::load_shader_module(blur_y_.fragment.c_str(), gpu_.device, &blur_y_shader);
  VkShaderModule final_shader;
  vkutil::load_shader_module(final_.fragment.c_str(), gpu_.device, &final_shader);

  // Bright Pass
  {
    bright_pass_.descriptor_layouts_.emplace_back(
        descriptor_layout_builder()
            .add_binding(10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                         VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(gpu_.device));
    VkPipelineLayoutCreateInfo pipeline_layout_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .setLayoutCount = static_cast<uint32_t>(bright_pass_.descriptor_layouts_.size()),
        .pSetLayouts = bright_pass_.descriptor_layouts_.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range,
    };
    VK_CHECK(vkCreatePipelineLayout(gpu_.device, &pipeline_layout_create_info, nullptr,
                                    &bright_pass_.pipeline_layout_));

    bright_pass_.pipeline_
        = PipelineBuilder()
              .set_shaders(vertex_shader, bright_pass_shader)
              .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
              .set_polygon_mode(VK_POLYGON_MODE_FILL)
              .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
              .set_multisampling_none()
              .disable_blending()
              .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
              .set_color_attachment_format(hdr_buffer_.get_write_color_image().imageFormat)
              .set_depth_format(hdr_buffer_.get_write_depth_image().imageFormat)
              .set_pipeline_layout(bright_pass_.pipeline_layout_)
              .build_pipeline(gpu_.device);
  }

  /*
  // SSAO Filter
  {
    adaptation_.descriptor_layouts_.emplace_back(
        descriptor_layout_builder()
            .add_binding(10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                         VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_binding(11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                         VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(gpu_.device));
    VkPipelineLayoutCreateInfo pipeline_layout_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .setLayoutCount = static_cast<uint32_t>(adaptation_.descriptor_layouts_.size()),
        .pSetLayouts = adaptation_.descriptor_layouts_.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range,
    };
    VK_CHECK(vkCreatePipelineLayout(gpu_.device, &pipeline_layout_create_info, nullptr,
                                    &adaptation_.pipeline_layout_));

    adaptation_.pipeline_
        = PipelineBuilder()
              .set_shaders(vertex_shader, adaptation_shader)
              .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
              .set_polygon_mode(VK_POLYGON_MODE_FILL)
              .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
              .set_multisampling_none()
              .disable_blending()
              .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
              .set_color_attachment_format(ssao_buffer_.get_write_buffer().color_image.imageFormat)
              .set_depth_format(ssao_buffer_.get_write_buffer().depth_image.imageFormat)
              .set_pipeline_layout(adaptation_.pipeline_layout_)
              .build_pipeline(gpu_.device);
  }
  */

  // SSAO Blur X
  {
    blur_x_.descriptor_layouts_.emplace_back(
        descriptor_layout_builder()
            .add_binding(10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                         VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(gpu_.device));
    VkPipelineLayoutCreateInfo pipeline_layout_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .setLayoutCount = static_cast<uint32_t>(blur_x_.descriptor_layouts_.size()),
        .pSetLayouts = blur_x_.descriptor_layouts_.data(),
    };
    VK_CHECK(vkCreatePipelineLayout(gpu_.device, &pipeline_layout_create_info, nullptr,
                                    &blur_x_.pipeline_layout_));

    blur_x_.pipeline_
        = PipelineBuilder()
              .set_shaders(vertex_shader, blur_x_shader)
              .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
              .set_polygon_mode(VK_POLYGON_MODE_FILL)
              .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
              .set_multisampling_none()
              .disable_blending()
              .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
              .set_color_attachment_format(hdr_buffer_.get_write_color_image().imageFormat)
              .set_depth_format(hdr_buffer_.get_write_depth_image().imageFormat)
              .set_pipeline_layout(blur_x_.pipeline_layout_)
              .build_pipeline(gpu_.device);
  }

  // SSAO Blur Y
  {
    blur_y_.descriptor_layouts_.emplace_back(
        descriptor_layout_builder()
            .add_binding(10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                         VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(gpu_.device));
    VkPipelineLayoutCreateInfo pipeline_layout_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .setLayoutCount = static_cast<uint32_t>(blur_y_.descriptor_layouts_.size()),
        .pSetLayouts = blur_y_.descriptor_layouts_.data(),
    };
    VK_CHECK(vkCreatePipelineLayout(gpu_.device, &pipeline_layout_create_info, nullptr,
                                    &blur_y_.pipeline_layout_));

    blur_y_.pipeline_
        = PipelineBuilder()
              .set_shaders(vertex_shader, blur_y_shader)
              .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
              .set_polygon_mode(VK_POLYGON_MODE_FILL)
              .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
              .set_multisampling_none()
              .disable_blending()
              .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
              .set_color_attachment_format(hdr_buffer_.get_write_color_image().imageFormat)
              .set_depth_format(hdr_buffer_.get_write_depth_image().imageFormat)
              .set_pipeline_layout(blur_y_.pipeline_layout_)
              .build_pipeline(gpu_.device);

  }

  // SSAO Final
  {
    final_.descriptor_layouts_.emplace_back(
        descriptor_layout_builder()
            .add_binding(10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                         VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_binding(11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                         VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(gpu_.device));
    VkPipelineLayoutCreateInfo pipeline_layout_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .setLayoutCount = static_cast<uint32_t>(final_.descriptor_layouts_.size()),
        .pSetLayouts = final_.descriptor_layouts_.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range,
    };
    VK_CHECK(vkCreatePipelineLayout(gpu_.device, &pipeline_layout_create_info, nullptr,
                                    &final_.pipeline_layout_));

    final_.pipeline_
        = PipelineBuilder()
              .set_shaders(vertex_shader, final_shader)
              .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
              .set_polygon_mode(VK_POLYGON_MODE_FILL)
              .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
              .set_multisampling_none()
              .disable_blending()
              .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
              .set_color_attachment_format(
                  renderer_->frame_buffer_.get_write_color_image().imageFormat)
              .set_depth_format(renderer_->frame_buffer_.get_write_depth_image().imageFormat)
              .set_pipeline_layout(final_.pipeline_layout_)
              .build_pipeline(gpu_.device);
  }

  vkDestroyShaderModule(gpu_.device, vertex_shader, nullptr);
  vkDestroyShaderModule(gpu_.device, bright_pass_shader, nullptr);
  vkDestroyShaderModule(gpu_.device, adaptation_shader, nullptr);
  vkDestroyShaderModule(gpu_.device, streaks_shader, nullptr);
  vkDestroyShaderModule(gpu_.device, blur_x_shader, nullptr);
  vkDestroyShaderModule(gpu_.device, blur_y_shader, nullptr);
  vkDestroyShaderModule(gpu_.device, final_shader, nullptr);
}

void hdr_pass::execute_blur_x(const VkCommandBuffer cmd) {
  blur_x_.descriptor_set_ = renderer_->get_current_frame().descriptor_pool.allocate(
      gpu_.device, blur_x_.descriptor_layouts_.at(0));
  hdr_buffer_.switch_buffers();

  vkutil::transition_read(cmd, hdr_buffer_.get_read_color_image());
  vkutil::transition_write(cmd, hdr_buffer_.get_write_color_image());
  vkutil::transition_write(cmd, hdr_buffer_.get_write_depth_image());

  VkRenderingAttachmentInfo newColorAttachment = vkinit::attachment_info(
      hdr_buffer_.get_write_color_image().imageView,
                                               nullptr, VK_IMAGE_LAYOUT_GENERAL);
  VkRenderingAttachmentInfo newDepthAttachment
      = vkinit::depth_attachment_info(hdr_buffer_.get_write_depth_image().imageView, nullptr,
                                      VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  VkRenderingInfo newRenderInfo = vkinit::rendering_info({effect_size_, effect_size_},
                                                        &newColorAttachment,
                                         &newDepthAttachment);
  vkCmdBeginRendering(cmd, &newRenderInfo);

  writer.clear();
  writer.write_image(
      10, hdr_buffer_.get_read_color_image().imageView,
      resource_manager_->get_database().get_sampler(0), // todo default_sampler_nearest
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.update_set(gpu_.device, blur_x_.descriptor_set_);

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blur_x_.pipeline_layout_, 0, 1,
                          &blur_x_.descriptor_set_, 0, nullptr);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blur_x_.pipeline_);
  viewport_.width = static_cast<float>(effect_size_);
  viewport_.height = static_cast<float>(effect_size_);
  scissor_.extent.width = effect_size_;
  scissor_.extent.height = effect_size_;
  vkCmdSetViewport(cmd, 0, 1, &viewport_);
  vkCmdSetScissor(cmd, 0, 1, &scissor_);
  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdEndRendering(cmd);
}

void hdr_pass::execute_blur_y(const VkCommandBuffer cmd) {
  blur_y_.descriptor_set_ = renderer_->get_current_frame().descriptor_pool.allocate(
      gpu_.device, blur_y_.descriptor_layouts_.at(0));
  hdr_buffer_.switch_buffers();

  vkutil::transition_read(cmd, hdr_buffer_.get_read_color_image());
  vkutil::transition_write(cmd, hdr_buffer_.get_write_color_image());
  vkutil::transition_write(cmd, hdr_buffer_.get_write_depth_image());

  VkRenderingAttachmentInfo newColorAttachment = vkinit::attachment_info(
      hdr_buffer_.get_write_color_image().imageView,
                                               nullptr, VK_IMAGE_LAYOUT_GENERAL);
  VkRenderingAttachmentInfo newDepthAttachment
      = vkinit::depth_attachment_info(hdr_buffer_.get_write_depth_image().imageView, nullptr,
                                      VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  VkRenderingInfo newRenderInfo = vkinit::rendering_info({effect_size_, effect_size_},
                                                        &newColorAttachment,
                                         &newDepthAttachment);
  vkCmdBeginRendering(cmd, &newRenderInfo);

  writer.clear();
  writer.write_image(
      10, hdr_buffer_.get_read_color_image().imageView,
      resource_manager_->get_database().get_sampler(0), // todo default_sampler_nearest
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.update_set(gpu_.device, blur_y_.descriptor_set_);

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blur_y_.pipeline_layout_, 0, 1,
                          &blur_y_.descriptor_set_, 0, nullptr);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, blur_y_.pipeline_);

  viewport_.width = static_cast<float>(effect_size_);
  viewport_.height = static_cast<float>(effect_size_);

  scissor_.extent.width = effect_size_;
  scissor_.extent.height = effect_size_;
  vkCmdSetViewport(cmd, 0, 1, &viewport_);
  vkCmdSetScissor(cmd, 0, 1, &scissor_);
  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdEndRendering(cmd);
}

void hdr_pass::execute(const VkCommandBuffer cmd) {

  bright_pass_.descriptor_set_ = renderer_->get_current_frame().descriptor_pool.allocate(
      gpu_.device, bright_pass_.descriptor_layouts_.at(0));
  renderer_->frame_buffer_.switch_buffers();

  vkutil::transition_read(cmd, renderer_->frame_buffer_.get_read_color_image());
  vkutil::transition_write(cmd, hdr_buffer_.get_write_color_image());
  vkutil::transition_write(cmd, hdr_buffer_.get_write_depth_image());

  VkRenderingAttachmentInfo newColorAttachment = vkinit::attachment_info(
      hdr_buffer_.get_write_color_image().imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
  VkRenderingAttachmentInfo newDepthAttachment
      = vkinit::depth_attachment_info(hdr_buffer_.get_write_depth_image().imageView, nullptr,
                                      VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  VkRenderingInfo newRenderInfo = vkinit::rendering_info({effect_size_, effect_size_},
                                                         &newColorAttachment, &newDepthAttachment);
  vkCmdBeginRendering(cmd, &newRenderInfo);

  writer.clear();
  writer.write_image(
      10, renderer_->frame_buffer_.get_read_color_image().imageView,
      resource_manager_->get_database().get_sampler(0),  // todo default_sampler_nearest
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.update_set(gpu_.device, bright_pass_.descriptor_set_);

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bright_pass_.pipeline_layout_, 0, 1,
                          &bright_pass_.descriptor_set_, 0, nullptr);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bright_pass_.pipeline_);

  viewport_.width = static_cast<float>(effect_size_);
  viewport_.height = static_cast<float>(effect_size_);
  scissor_.extent.width = effect_size_;
  scissor_.extent.height = effect_size_;

  vkCmdSetViewport(cmd, 0, 1, &viewport_);


  vkCmdSetScissor(cmd, 0, 1, &scissor_);
  vkCmdDraw(cmd, 3, 1, 0, 0);

  vkCmdEndRendering(cmd);

  for (int i = 0; i < renderer_->get_config().bloom_quality; i++) {
    execute_blur_x(cmd);
    execute_blur_y(cmd);
  }

  // FINAL PASS
  final_.descriptor_set_ = renderer_->get_current_frame().descriptor_pool.allocate(
      gpu_.device, final_.descriptor_layouts_.at(0));

  hdr_buffer_.switch_buffers();

  vkutil::transition_read(cmd, hdr_buffer_.get_read_color_image());
  vkutil::transition_write(cmd, renderer_->frame_buffer_.get_write_color_image());
  vkutil::transition_write(cmd, renderer_->frame_buffer_.get_write_depth_image());

  newColorAttachment
      = vkinit::attachment_info(renderer_->frame_buffer_.get_write_color_image().imageView,
                                nullptr, VK_IMAGE_LAYOUT_GENERAL);
  newDepthAttachment = vkinit::depth_attachment_info(
      renderer_->frame_buffer_.get_write_depth_image().imageView, nullptr,
      VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  newRenderInfo = vkinit::rendering_info(renderer_->get_window().extent, &newColorAttachment,
                                         &newDepthAttachment);
  vkCmdBeginRendering(cmd, &newRenderInfo);

  writer.clear();
  writer.write_image(
      10, renderer_->frame_buffer_.get_read_color_image().imageView,
      resource_manager_->get_database().get_sampler(0),  // todo default_sampler_nearest
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.write_image(
      11, hdr_buffer_.get_read_color_image().imageView,
      resource_manager_->get_database().get_sampler(1),  // todo default_sampler_linear
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.update_set(gpu_.device, final_.descriptor_set_);

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, final_.pipeline_layout_, 0, 1,
                          &final_.descriptor_set_, 0, nullptr);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, final_.pipeline_);

  viewport_.width = static_cast<float>(renderer_->get_window().extent.width);
  viewport_.height = static_cast<float>(renderer_->get_window().extent.height);
  scissor_.extent.width = renderer_->get_window().extent.width;
  scissor_.extent.height = renderer_->get_window().extent.height;
  vkCmdSetViewport(cmd, 0, 1, &viewport_);
  vkCmdSetScissor(cmd, 0, 1, &scissor_);
  vkCmdPushConstants(cmd, final_.pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                     sizeof(render_config::hdr_params), &renderer_->get_config().hdr);
  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdEndRendering(cmd);
}

void hdr_pass::cleanup() {
  vkDestroyPipelineLayout(gpu_.device, pipeline_layout_, nullptr);
  vkDestroyPipeline(gpu_.device, pipeline_, nullptr);
}
