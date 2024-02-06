#include "ssao_pass.h"

#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"
#include "vk_renderer.h"


void ssao_pass::prepare() {

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
  std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes
      = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};

  descriptorPool_.init(gpu_.device, 1, sizes);

  VkShaderModule vertex_shader;
  vkutil::load_shader_module(vertex_shader_source_.c_str(), gpu_.device, &vertex_shader);
  VkShaderModule fragment_shader;
  vkutil::load_shader_module(fragment_shader_source_.c_str(), gpu_.device, &fragment_shader);

  pipeline_ = PipelineBuilder()
                  .set_shaders(vertex_shader, fragment_shader)
                  .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                  .set_polygon_mode(VK_POLYGON_MODE_FILL)
                  .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                  .set_multisampling_none()
                  .disable_blending()
                  .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
                  .set_color_attachment_format(renderer_->frame_buffer_.get_write_buffer().color_image.imageFormat)
                  .set_depth_format(renderer_->frame_buffer_.get_write_buffer().depth_image.imageFormat)
                  .set_pipeline_layout(pipeline_layout_)
                  .build_pipeline(gpu_.device);

  // Clean up shader modules after pipeline creation
  vkDestroyShaderModule(gpu_.device, vertex_shader, nullptr);
  vkDestroyShaderModule(gpu_.device, fragment_shader, nullptr);
}

void ssao_pass::execute(const VkCommandBuffer cmd) {
  vkCmdEndRendering(cmd);

  renderer_->frame_buffer_.switch_buffers();

  vkutil::transition_image(cmd, renderer_->frame_buffer_.get_read_buffer().color_image.image,
                           VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  vkutil::transition_image(cmd, renderer_->frame_buffer_.get_write_buffer().color_image.image,
                           VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
  vkutil::transition_image(cmd, renderer_->frame_buffer_.get_write_buffer().depth_image.image,
                           VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  VkRenderingAttachmentInfo newColorAttachment = vkinit::attachment_info(renderer_->frame_buffer_.get_write_buffer().color_image.imageView,
                                nullptr, VK_IMAGE_LAYOUT_GENERAL);
  VkRenderingAttachmentInfo newDepthAttachment = vkinit::depth_attachment_info(
      renderer_->frame_buffer_.get_write_buffer().depth_image.imageView,
                                      VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  VkRenderingInfo newRenderInfo
      = vkinit::rendering_info(renderer_->get_window().extent, &newColorAttachment, &newDepthAttachment);
  vkCmdBeginRendering(cmd, &newRenderInfo);


  auto descriptor_set = descriptorPool_.allocate(gpu_.device, descriptor_layouts_.at(0));
  descriptor_writer writer;
  writer.clear();
  writer.write_image(10, renderer_->frame_buffer_.get_read_buffer().color_image.imageView,
                     resource_manager_->get_database().get_sampler(0), //todo default_sampler_nearest
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.update_set(gpu_.device, descriptor_set);

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                          &descriptor_set, 0, nullptr);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
  VkViewport viewport = {};
  viewport.x = 0;
  viewport.y = 0;
  viewport.width = static_cast<float>(renderer_->get_window().extent.width);
  viewport.height = static_cast<float>(renderer_->get_window().extent.height);
  viewport.minDepth = 0.f;
  viewport.maxDepth = 1.f;

  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = {};
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent.width = renderer_->get_window().extent.width;
  scissor.extent.height = renderer_->get_window().extent.height;

  vkCmdSetScissor(cmd, 0, 1, &scissor);
  vkCmdDraw(cmd, 3, 1, 0, 0);
}

void ssao_pass::cleanup() {
  vkDestroyPipelineLayout(gpu_.device, pipeline_layout_, nullptr);
  vkDestroyPipeline(gpu_.device, pipeline_, nullptr);
}
