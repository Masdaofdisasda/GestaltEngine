#include "skybox_pass.h"

#include "vk_pipelines.h"
#include "vk_renderer.h"

void skybox_pass::init(vk_renderer& renderer) {
  gpu_ = renderer.gpu_;
  renderer_ = &renderer;

  std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
  };

  descriptor_allocator_.init(gpu_.device, 3, sizes);

  {
    descriptor_layout_
        = descriptor_layout_builder()
              .add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
              .add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_FRAGMENT_BIT)
              .build(gpu_.device);
  }

  VkPipelineLayoutCreateInfo pipeline_layout_create_info{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .setLayoutCount = 1,
      .pSetLayouts = &descriptor_layout_,
  };

  VK_CHECK(vkCreatePipelineLayout(gpu_.device, &pipeline_layout_create_info, nullptr,
                                  &pipeline_layout_));

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
                  .set_color_attachment_format(renderer_->frame_buffer_.color_image.imageFormat)
                  .set_depth_format(renderer_->frame_buffer_.depth_image.imageFormat)
                  .set_pipeline_layout(pipeline_layout_)
                  .build_pipeline(gpu_.device);

  // Clean up shader modules after pipeline creation
  vkDestroyShaderModule(gpu_.device, vertex_shader, nullptr);
  vkDestroyShaderModule(gpu_.device, fragment_shader, nullptr);
}

void skybox_pass::cleanup() {
  descriptor_allocator_.destroy_pools(gpu_.device);

  vkDestroyPipelineLayout(gpu_.device, pipeline_layout_, nullptr);
  vkDestroyPipeline(gpu_.device, pipeline_, nullptr);

  vkDestroyDescriptorSetLayout(gpu_.device, descriptor_layout_, nullptr);
}

void skybox_pass::execute(const VkCommandBuffer cmd) {
  descriptor_set_ = descriptor_allocator_.allocate(gpu_.device, descriptor_layout_);

  writer_.clear();
  writer_.write_buffer(0, renderer_->resource_manager_->per_frame_data_buffer.buffer,
                      sizeof(per_frame_data), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  writer_.write_image(1, renderer_->resource_manager_->ibl_data.environment_map.imageView,
                     renderer_->resource_manager_->ibl_data.cube_map_sampler, VK_IMAGE_LAYOUT_GENERAL,
                     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer_.update_set(gpu_.device, descriptor_set_);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                          &descriptor_set_, 0, nullptr);
  VkViewport viewport = {};
  viewport.x = 0;
  viewport.y = 0;
  viewport.width = static_cast<float>(renderer_->window_.extent.width);
  viewport.height = static_cast<float>(renderer_->window_.extent.height);
  viewport.minDepth = 0.f;
  viewport.maxDepth = 1.f;

  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = {};
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent.width = renderer_->window_.extent.width;
  scissor.extent.height = renderer_->window_.extent.height;

  vkCmdSetScissor(cmd, 0, 1, &scissor);
  vkCmdDraw(cmd, 36, 1, 0, 0);  // 36 vertices for the cube
}