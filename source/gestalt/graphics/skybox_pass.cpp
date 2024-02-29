#include "skybox_pass.h"

#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"

void skybox_pass::prepare() {
  fmt::print("Preparing skybox pass\n");
  descriptor_layouts_.push_back(resource_manager_->per_frame_data_layout);
  descriptor_layouts_.push_back(resource_manager_->ibl_data.IblLayout);

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
  VkShaderModule fragment_shader;
  vkutil::load_shader_module(fragment_shader_source_.c_str(), gpu_.device, &fragment_shader);

  const auto color_image = resource_manager_->get_resource<AllocatedImage>("scene_color");
  const auto depth_image = resource_manager_->get_resource<AllocatedImage>("scene_depth");

  pipeline_ = PipelineBuilder()
                  .set_shaders(vertex_shader, fragment_shader)
                  .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                  .set_polygon_mode(VK_POLYGON_MODE_FILL)
                  .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                  .set_multisampling_none()
                  .disable_blending()
                  .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
                  .set_color_attachment_format(color_image->imageFormat)
                  .set_depth_format(depth_image->imageFormat)
                  .set_pipeline_layout(pipeline_layout_)
                  .build_pipeline(gpu_.device);

  // Clean up shader modules after pipeline creation
  vkDestroyShaderModule(gpu_.device, vertex_shader, nullptr);
  vkDestroyShaderModule(gpu_.device, fragment_shader, nullptr);
}

void skybox_pass::execute(const VkCommandBuffer cmd) {
  const auto color_image = resource_manager_->get_resource<AllocatedImage>("scene_color");
  const auto depth_image = resource_manager_->get_resource<AllocatedImage>("scene_depth");

  VkRenderingAttachmentInfo colorAttachment
      = vkinit::attachment_info(color_image->imageView, nullptr, color_image->currentLayout);
  VkClearValue depth_clear = {.depthStencil = {1.f, 0}};
  VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(
      depth_image->imageView, &depth_clear, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  VkRenderingInfo renderInfo
      = vkinit::rendering_info(color_image->getExtent2D(),
                               &colorAttachment, &depthAttachment);

  vkCmdBeginRendering(cmd, &renderInfo);

  auto view = resource_manager_->get_database().get_camera(0).view_matrix;
  auto projection = resource_manager_->get_database().get_camera(0).projection_matrix;
  resource_manager_->per_frame_data_.proj = projection;
  resource_manager_->per_frame_data_.view = view;
  resource_manager_->per_frame_data_.viewproj = projection * view;

  auto& light = resource_manager_->get_database().get_light(0);
  resource_manager_->per_frame_data_.dir_light_direction = light.direction;
  resource_manager_->per_frame_data_.dir_light_intensity = light.intensity;

  // Define the dimensions of the orthographic projection
  auto config = resource_manager_->config_.shadow;
  glm::vec3 direction = normalize(light.direction);  // Ensure the direction is normalized

  glm::vec3 up = glm::vec3(0, 1, 0);
  if (glm::abs(dot(up, direction)) > 0.999f) {
    //up = glm::vec3(1, 0, 0);  // Switch to a different up vector if the initial choice is parallel
  }

  // Create a view matrix for the light
  glm::mat4 lightView = lookAt(glm::vec3(0, 0, 0), -direction, glm::vec3(0, 0, 1));

  glm::vec3 min{config.bottom};
  glm::vec3 max{config.top};

  glm::vec3 corners[] = {
      glm::vec3(min.x, min.y, min.z), glm::vec3(min.x, max.y, min.z),
      glm::vec3(min.x, min.y, max.z), glm::vec3(min.x, max.y, max.z),
      glm::vec3(max.x, min.y, min.z), glm::vec3(max.x, max.y, min.z),
      glm::vec3(max.x, min.y, max.z), glm::vec3(max.x, max.y, max.z),
  };
  for (auto& v : corners) v = glm::vec3(lightView * glm::vec4(v, 1.0f));

  glm::vec3 vmin(std::numeric_limits<float>::max());
  glm::vec3 vmax(std::numeric_limits<float>::lowest());

  for (auto& corner : corners) {
    vmin = glm::min(vmin, corner);
    vmax = glm::max(vmax, corner);
  }
  glm::mat4 lightProjection = glm::orthoRH_ZO(min.x, max.x, min.y, max.y, -max.z, -min.z);

  lightProjection[1][1] *= -1;

  resource_manager_->per_frame_data_.light_view_proj = lightProjection * lightView;

  void* mapped_data;
  VmaAllocation allocation = resource_manager_->per_frame_data_buffer.allocation;
  VK_CHECK(vmaMapMemory(gpu_.allocator, allocation, &mapped_data));
  const auto scene_uniform_data = static_cast<per_frame_data*>(mapped_data);
  *scene_uniform_data = resource_manager_->per_frame_data_;

  VkDescriptorBufferInfo buffer_info;
  buffer_info.buffer = resource_manager_->per_frame_data_buffer.buffer;
  buffer_info.offset = 0;
  buffer_info.range = sizeof(per_frame_data);

  VkWriteDescriptorSet descriptor_write = {};
  descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptor_write.dstBinding = 0;
  descriptor_write.dstArrayElement = 0;
  descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  descriptor_write.descriptorCount = 1;
  descriptor_write.pBufferInfo = &buffer_info;

  gpu_.vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                            &descriptor_write);


  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 1, 1,
                          &resource_manager_->ibl_data.IblSet, 0, nullptr);
  VkViewport viewport = {};
  viewport.x = 0;
  viewport.y = 0;
  viewport.width = static_cast<float>(color_image->getExtent2D().width);
  viewport.height = static_cast<float>(color_image->getExtent2D().height);
  viewport.minDepth = 0.f;
  viewport.maxDepth = 1.f;

  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = {};
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent.width = color_image->getExtent2D().width;
  scissor.extent.height = color_image->getExtent2D().height;

  vkCmdSetScissor(cmd, 0, 1, &scissor);
  vkCmdDraw(cmd, 36, 1, 0, 0);  // 36 vertices for the cube

  vkCmdEndRendering(cmd);
  
  vmaUnmapMemory(gpu_.allocator, resource_manager_->per_frame_data_buffer.allocation);
}

void skybox_pass::cleanup() {
  vkDestroyPipelineLayout(gpu_.device, pipeline_layout_, nullptr);
  vkDestroyPipeline(gpu_.device, pipeline_, nullptr);
}