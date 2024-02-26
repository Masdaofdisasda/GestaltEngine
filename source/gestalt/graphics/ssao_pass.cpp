#include "ssao_pass.h"

#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"
#include "vk_renderer.h"


void ssao_pass::prepare_filter_pass(VkShaderModule vertex_shader, VkShaderModule ssao_filter_shader) {
  filter_.descriptor_layouts_.emplace_back(
      descriptor_layout_builder()
      .add_binding(10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   VK_SHADER_STAGE_FRAGMENT_BIT)
      .add_binding(11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                   VK_SHADER_STAGE_FRAGMENT_BIT)
      .build(gpu_.device));
  VkPipelineLayoutCreateInfo pipeline_layout_create_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .setLayoutCount = static_cast<uint32_t>(filter_.descriptor_layouts_.size()),
      .pSetLayouts = filter_.descriptor_layouts_.data(),
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &push_constant_range_,
  };
  VK_CHECK(vkCreatePipelineLayout(gpu_.device, &pipeline_layout_create_info, nullptr,
    &filter_.pipeline_layout_));

  filter_.pipeline_
      = PipelineBuilder()
        .set_shaders(vertex_shader, ssao_filter_shader)
        .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .set_polygon_mode(VK_POLYGON_MODE_FILL)
        .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .set_multisampling_none()
        .disable_blending()
        .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
        .set_color_attachment_format(ssao_buffer_.get_write_color_image().imageFormat)
        .set_depth_format(ssao_buffer_.get_write_depth_image().imageFormat)
        .set_pipeline_layout(filter_.pipeline_layout_)
        .build_pipeline(gpu_.device);
}

void ssao_pass::prepare_blur_x(VkShaderModule vertex_shader, VkShaderModule ssao_blur_x_shader) {
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
        .set_shaders(vertex_shader, ssao_blur_x_shader)
        .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .set_polygon_mode(VK_POLYGON_MODE_FILL)
        .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .set_multisampling_none()
        .disable_blending()
        .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
        .set_color_attachment_format(ssao_buffer_.get_write_color_image().imageFormat)
        .set_depth_format(ssao_buffer_.get_write_depth_image().imageFormat)
        .set_pipeline_layout(blur_x_.pipeline_layout_)
        .build_pipeline(gpu_.device);
}

void ssao_pass::prepare_blur_y(VkShaderModule vertex_shader, VkShaderModule ssao_blur_y_shader) {
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
        .set_shaders(vertex_shader, ssao_blur_y_shader)
        .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .set_polygon_mode(VK_POLYGON_MODE_FILL)
        .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .set_multisampling_none()
        .disable_blending()
        .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
        .set_color_attachment_format(ssao_buffer_.get_write_color_image().imageFormat)
        .set_depth_format(ssao_buffer_.get_write_depth_image().imageFormat)
        .set_pipeline_layout(blur_y_.pipeline_layout_)
            .build_pipeline(gpu_.device);
}

void ssao_pass::prepare_final(VkShaderModule vertex_shader, VkShaderModule ssao_final_shader) {
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
      .pPushConstantRanges = &push_constant_range_,
  };
  VK_CHECK(vkCreatePipelineLayout(gpu_.device, &pipeline_layout_create_info, nullptr,
    &final_.pipeline_layout_));

  final_.pipeline_
      = PipelineBuilder()
        .set_shaders(vertex_shader, ssao_final_shader)
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

void ssao_pass::prepare() {
  resource_manager_->create_framebuffer({effect_size_, effect_size_, 1}, ssao_buffer_);
  rotation_pattern = resource_manager_->load_image("../../assets/rot_texture.bmp").value();

  VkShaderModule vertex_shader;
  vkutil::load_shader_module(vertex_ssao.c_str(), gpu_.device, &vertex_shader);
  VkShaderModule ssao_filter_shader;
  vkutil::load_shader_module(filter_.fragment.c_str(), gpu_.device, &ssao_filter_shader);
  VkShaderModule ssao_blur_x_shader;
  vkutil::load_shader_module(blur_x_.fragment.c_str(), gpu_.device, &ssao_blur_x_shader);
  VkShaderModule ssao_blur_y_shader;
  vkutil::load_shader_module(blur_y_.fragment.c_str(), gpu_.device, &ssao_blur_y_shader);
  VkShaderModule ssao_final_shader;
  vkutil::load_shader_module(final_.fragment.c_str(), gpu_.device, &ssao_final_shader);

  prepare_filter_pass(vertex_shader, ssao_filter_shader);
  prepare_blur_x(vertex_shader, ssao_blur_x_shader);
  prepare_blur_y(vertex_shader, ssao_blur_y_shader);
  prepare_final(vertex_shader, ssao_final_shader);

  vkDestroyShaderModule(gpu_.device, vertex_shader, nullptr);
  vkDestroyShaderModule(gpu_.device, ssao_filter_shader, nullptr);
  vkDestroyShaderModule(gpu_.device, ssao_blur_x_shader, nullptr);
  vkDestroyShaderModule(gpu_.device, ssao_blur_y_shader, nullptr);
  vkDestroyShaderModule(gpu_.device, ssao_final_shader, nullptr);
}

void ssao_pass::execute_filter(const VkCommandBuffer cmd) {
  filter_.descriptor_set_ = renderer_->get_current_frame().descriptor_pool.allocate(
      gpu_.device, filter_.descriptor_layouts_.at(0));

  renderer_->frame_buffer_.switch_buffers();

  vkutil::transition_read(cmd, renderer_->frame_buffer_.get_read_depth_image());
  vkutil::transition_write(cmd, ssao_buffer_.get_write_color_image());
  vkutil::transition_write(cmd, ssao_buffer_.get_write_depth_image());

  VkRenderingAttachmentInfo newColorAttachment = vkinit::attachment_info(
      ssao_buffer_.get_write_color_image().imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
  VkRenderingAttachmentInfo newDepthAttachment
      = vkinit::depth_attachment_info(ssao_buffer_.get_write_depth_image().imageView,nullptr,
                                      VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  VkRenderingInfo newRenderInfo = vkinit::rendering_info({effect_size_, effect_size_},
                                                         &newColorAttachment, &newDepthAttachment);
  vkCmdBeginRendering(cmd, &newRenderInfo);

  writer.clear();
  writer.write_image(
      10, renderer_->frame_buffer_.get_read_depth_image().imageView,
      resource_manager_->get_database().get_sampler(0), // todo default_sampler_nearest
      VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.write_image(
      11, rotation_pattern.imageView,
      resource_manager_->get_database().get_sampler(0), // todo default_sampler_nearest
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.update_set(gpu_.device, filter_.descriptor_set_);

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, filter_.pipeline_layout_, 0, 1,
                          &filter_.descriptor_set_, 0, nullptr);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, filter_.pipeline_);

  viewport_.width = static_cast<float>(effect_size_);
  viewport_.height = static_cast<float>(effect_size_);
  scissor_.extent.width = effect_size_;
  scissor_.extent.height = effect_size_;
  vkCmdSetViewport(cmd, 0, 1, &viewport_);
  vkCmdSetScissor(cmd, 0, 1, &scissor_);

  vkCmdPushConstants(cmd, filter_.pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                     sizeof(render_config::ssao_params), &renderer_->get_config().ssao);
  vkCmdDraw(cmd, 3, 1, 0, 0);

  vkCmdEndRendering(cmd);
}

void ssao_pass::execute_blur_x(const VkCommandBuffer cmd) {
  blur_x_.descriptor_set_ = renderer_->get_current_frame().descriptor_pool.allocate(
      gpu_.device, blur_x_.descriptor_layouts_.at(0));

  ssao_buffer_.switch_buffers();

  vkutil::transition_read(cmd, ssao_buffer_.get_read_color_image());
  vkutil::transition_write(cmd, ssao_buffer_.get_write_color_image());
  vkutil::transition_write(cmd, ssao_buffer_.get_write_depth_image());

  VkRenderingAttachmentInfo newColorAttachment = vkinit::attachment_info(
      ssao_buffer_.get_write_color_image().imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
  VkRenderingAttachmentInfo newDepthAttachment
      = vkinit::depth_attachment_info(ssao_buffer_.get_write_depth_image().imageView,
                                      nullptr,
                                      VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  VkRenderingInfo newRenderInfo = vkinit::rendering_info({effect_size_, effect_size_},
                                                         &newColorAttachment,
                                         &newDepthAttachment);
  vkCmdBeginRendering(cmd, &newRenderInfo);

  writer.clear();
  writer.write_image(
      10, ssao_buffer_.get_read_color_image().imageView,
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

void ssao_pass::execute_blur_y(const VkCommandBuffer cmd) {
  blur_y_.descriptor_set_ = renderer_->get_current_frame().descriptor_pool.allocate(
      gpu_.device, blur_y_.descriptor_layouts_.at(0));

  ssao_buffer_.switch_buffers();

  vkutil:: transition_read(cmd, ssao_buffer_.get_read_color_image());
  vkutil::transition_write(cmd, ssao_buffer_.get_write_color_image());
  vkutil::transition_write(cmd, ssao_buffer_.get_write_depth_image());

  VkRenderingAttachmentInfo newColorAttachment = vkinit::attachment_info(
      ssao_buffer_.get_write_color_image().imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
  VkRenderingAttachmentInfo newDepthAttachment
      = vkinit::depth_attachment_info(ssao_buffer_.get_write_depth_image().imageView,
                                      nullptr,
                                      VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  VkRenderingInfo newRenderInfo = vkinit::rendering_info({effect_size_, effect_size_},
                                                         &newColorAttachment,
                                         &newDepthAttachment);
  vkCmdBeginRendering(cmd, &newRenderInfo);

  writer.clear();
  writer.write_image(
      10, ssao_buffer_.get_read_color_image().imageView,
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

void ssao_pass::execute_final(const VkCommandBuffer cmd) {
  final_.descriptor_set_ = renderer_->get_current_frame().descriptor_pool.allocate(gpu_.device, final_.descriptor_layouts_.at(0));

  ssao_buffer_.switch_buffers();

  vkutil::transition_read(cmd, ssao_buffer_.get_read_color_image());
  vkutil::transition_read(cmd, renderer_->frame_buffer_.get_read_color_image());
  vkutil::transition_write(cmd, renderer_->frame_buffer_.get_write_color_image());
  vkutil::transition_write(cmd, renderer_->frame_buffer_.get_write_depth_image());

  VkRenderingAttachmentInfo newColorAttachment
      = vkinit::attachment_info(renderer_->frame_buffer_.get_write_color_image().imageView,
                                nullptr, VK_IMAGE_LAYOUT_GENERAL);
  VkRenderingAttachmentInfo newDepthAttachment = vkinit::depth_attachment_info(
      renderer_->frame_buffer_.get_write_depth_image().imageView, nullptr,
      VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  VkRenderingInfo newRenderInfo = vkinit::rendering_info(renderer_->get_window().extent,
                                                         &newColorAttachment, &newDepthAttachment);
  vkCmdBeginRendering(cmd, &newRenderInfo);

  writer.clear();
  writer.write_image(
      10, renderer_->frame_buffer_.get_read_color_image().imageView,
      resource_manager_->get_database().get_sampler(0),  // todo default_sampler_nearest
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.write_image(
      11, ssao_buffer_.get_read_color_image().imageView,
      resource_manager_->get_database().get_sampler(0),  // todo default_sampler_nearest
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
                     sizeof(render_config::ssao_params), &renderer_->get_config().ssao);
  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdEndRendering(cmd);
}

void ssao_pass::execute(const VkCommandBuffer cmd) {

  execute_filter(cmd);

  for (int i = 0; i < renderer_->get_config().ssao_quality; ++i) {
    execute_blur_x(cmd);
    execute_blur_y(cmd);
  }

  execute_final(cmd);
}

void ssao_pass::cleanup() {
  vkDestroyPipelineLayout(gpu_.device, pipeline_layout_, nullptr);
  vkDestroyPipeline(gpu_.device, pipeline_, nullptr);
}


void ssao_filter_shader::prepare() {
  rotation_pattern = resource_manager_->load_image("../../assets/rot_texture.bmp").value();

  descriptor_layouts_.emplace_back(
      descriptor_layout_builder()
          .add_binding(10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
          .add_binding(11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
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

  const auto color_image = resource_manager_->get_resource<AllocatedImage>("color_ssao_filter");

  pipeline_
      = PipelineBuilder()
            .set_shaders(vertex_shader, ssao_filter_shader)
            .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .set_polygon_mode(VK_POLYGON_MODE_FILL)
            .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .set_multisampling_none()
            .disable_blending()
            .disable_depthtest()
            .set_color_attachment_format(color_image->imageFormat)
            .set_pipeline_layout(pipeline_layout_)
            .build_pipeline(gpu_.device);
}


void ssao_filter_shader::execute(VkCommandBuffer cmd) {

  descriptor_set_ = resource_manager_->descriptor_pool->allocate(
      gpu_.device, descriptor_layouts_.at(0));


  const auto scene_depth = resource_manager_->get_resource<AllocatedImage>("scene_transparent_depth");
  const auto color_image = resource_manager_->get_resource<AllocatedImage>("color_ssao_filter");

  VkRenderingAttachmentInfo newColorAttachment = vkinit::attachment_info(color_image->imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
  VkRenderingInfo newRenderInfo = vkinit::rendering_info({effect_size_, effect_size_},
                                                         &newColorAttachment, nullptr);
  vkCmdBeginRendering(cmd, &newRenderInfo);

  writer.clear();
  writer.write_image(
      10, scene_depth->imageView,
      resource_manager_->get_database().get_sampler(0),  // todo default_sampler_nearest
      VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.write_image(
      11, rotation_pattern.imageView,
      resource_manager_->get_database().get_sampler(0),  // todo default_sampler_nearest
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.update_set(gpu_.device, descriptor_set_);

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                          &descriptor_set_, 0, nullptr);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

  vkCmdSetViewport(cmd, 0, 1, &viewport_);
  vkCmdSetScissor(cmd, 0, 1, &scissor_);

  vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                     sizeof(render_config::ssao_params), &resource_manager_->config_.ssao);
  vkCmdDraw(cmd, 3, 1, 0, 0);

  vkCmdEndRendering(cmd);
}

void ssao_filter_shader::cleanup() {
   vkDestroyPipelineLayout(gpu_.device, pipeline_layout_, nullptr);
  vkDestroyPipeline(gpu_.device, pipeline_, nullptr);
}


void ssao_blur_shader::prepare() {
  descriptor_layouts_.emplace_back(
      descriptor_layout_builder()
          .add_binding(10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
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

  const auto color_image = resource_manager_->get_resource<AllocatedImage>("color_ssao_filtered");

  PipelineBuilder builder
      = PipelineBuilder()
        .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .set_polygon_mode(VK_POLYGON_MODE_FILL)
        .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .set_multisampling_none()
        .disable_blending()
        .disable_depthtest()
        .set_color_attachment_format(color_image->imageFormat)
        .set_pipeline_layout(pipeline_layout_);

  blur_x_pipeline_ = builder.set_shaders(vertex_shader, ssao_blur_x_shader).build_pipeline(gpu_.device);
  blur_y_pipeline_ = builder.set_shaders(vertex_shader, ssao_blur_y_shader).build_pipeline(gpu_.device);
}

void ssao_blur_shader::execute(VkCommandBuffer cmd) {
  const auto image_x = resource_manager_->get_resource<AllocatedImage>("color_ssao_filtered");
  const auto image_y = resource_manager_->get_resource<AllocatedImage>("ssao_blur_y");

  for (int i = 0; i < resource_manager_->config_.ssao_quality; ++i) {
    {
      blur_x_descriptor_set_
          = resource_manager_->descriptor_pool->allocate(gpu_.device, descriptor_layouts_.at(0));
      vkutil::transition_read(cmd, *image_x);
      vkutil::transition_write(cmd, *image_y);

      VkRenderingAttachmentInfo newColorAttachment
          = vkinit::attachment_info(image_y->imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
      VkRenderingInfo newRenderInfo
          = vkinit::rendering_info({effect_size_, effect_size_}, &newColorAttachment, nullptr);
      vkCmdBeginRendering(cmd, &newRenderInfo);

      writer.clear();
      writer.write_image(
          10, image_x->imageView,
          resource_manager_->get_database().get_sampler(0),  // todo default_sampler_nearest
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
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
      blur_y_descriptor_set_
          = resource_manager_->descriptor_pool->allocate(gpu_.device, descriptor_layouts_.at(0));
      vkutil::transition_read(cmd, *image_y);
      vkutil::transition_write(cmd, *image_x);

      VkRenderingAttachmentInfo newColorAttachment
          = vkinit::attachment_info(image_x->imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
      VkRenderingInfo newRenderInfo
          = vkinit::rendering_info({effect_size_, effect_size_}, &newColorAttachment, nullptr);
      vkCmdBeginRendering(cmd, &newRenderInfo);

      writer.clear();
      writer.write_image(
          10, image_y->imageView,
          resource_manager_->get_database().get_sampler(0),  // todo default_sampler_nearest
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
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

void ssao_blur_shader::cleanup() {
  vkDestroyPipelineLayout(gpu_.device, pipeline_layout_, nullptr);
  vkDestroyPipeline(gpu_.device, blur_x_pipeline_, nullptr);
  vkDestroyPipeline(gpu_.device, blur_y_pipeline_, nullptr);
}

void ssao_final_shader::prepare() {
  descriptor_layouts_.emplace_back(
      descriptor_layout_builder()
          .add_binding(10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
          .add_binding(11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
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

  const auto color_image = resource_manager_->get_resource<AllocatedImage>("scene_ssao_final");

  pipeline_
      = PipelineBuilder()
            .set_shaders(vertex_shader, ssao_final_shader)
            .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .set_polygon_mode(VK_POLYGON_MODE_FILL)
            .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .set_multisampling_none()
            .disable_blending()
            .disable_depthtest()
            .set_color_attachment_format(color_image->imageFormat)
            .set_pipeline_layout(pipeline_layout_)
            .build_pipeline(gpu_.device);
}

void ssao_final_shader::execute(VkCommandBuffer cmd) {
  descriptor_set_ = resource_manager_->descriptor_pool->allocate(
      gpu_.device, descriptor_layouts_.at(0));

  
  const auto scene_color = resource_manager_->get_resource<AllocatedImage>("scene_transparent_color");
  const auto scene_ssao = resource_manager_->get_resource<AllocatedImage>("scene_ssao_final");
  const auto ssao_blurred = resource_manager_->get_resource<AllocatedImage>("ssao_blurred_final");

  VkRenderingAttachmentInfo newColorAttachment = vkinit::attachment_info(scene_ssao->imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
  VkRenderingInfo newRenderInfo = vkinit::rendering_info(scene_ssao->getExtent2D(),
                                                         &newColorAttachment, nullptr);
  vkCmdBeginRendering(cmd, &newRenderInfo);

  writer.clear();
  writer.write_image(
      10, scene_color->imageView,
      resource_manager_->get_database().get_sampler(0),  // todo default_sampler_nearest
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.write_image(
      11, ssao_blurred->imageView,
      resource_manager_->get_database().get_sampler(0),  // todo default_sampler_nearest
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
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
                     sizeof(render_config::ssao_params), &resource_manager_->config_.ssao);
  vkCmdDraw(cmd, 3, 1, 0, 0);
  vkCmdEndRendering(cmd);
}

void ssao_final_shader::cleanup() {
  vkDestroyPipelineLayout(gpu_.device, pipeline_layout_, nullptr);
  vkDestroyPipeline(gpu_.device, pipeline_, nullptr);
}


