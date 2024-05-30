#include "RenderPass.hpp"

#include "vk_images.hpp"
#include "vk_initializers.hpp"
#include "vk_pipelines.hpp"

namespace gestalt::graphics {

    void DeferredPass::prepare() {
      fmt::print("Preparing {}\n", get_name());

      const auto& per_frame_buffers = repository_->get_buffer<PerFrameDataBuffers>();
      const auto& ibl_buffers = repository_->get_buffer<IblBuffers>();
      const auto& mesh_buffers = repository_->get_buffer<MeshBuffers>();
      descriptor_layouts_.push_back(per_frame_buffers.descriptor_layout);
      descriptor_layouts_.push_back(ibl_buffers.descriptor_layout);
      descriptor_layouts_.push_back(repository_->material_data.resource_layout);
      descriptor_layouts_.push_back(repository_->material_data.constants_layout);
      descriptor_layouts_.push_back(mesh_buffers.descriptor_layout);

      dependencies_
          = RenderPassDependencyBuilder()
                .add_shader(ShaderStage::kVertex, "geometry.vert.spv")
                .add_shader(ShaderStage::kFragment, "geometry_deferred.frag.spv")
                .add_image_attachment(registry_->attachments_.gbuffer1, ImageUsageType::kWrite)
                .add_image_attachment(registry_->attachments_.gbuffer2, ImageUsageType::kWrite)
                .add_image_attachment(registry_->attachments_.gbuffer3, ImageUsageType::kWrite)
                .add_image_attachment(registry_->attachments_.scene_depth, ImageUsageType::kWrite, 0,
                                      ImageClearOperation::kClear)
                .set_push_constant_range(sizeof(GpuDrawPushConstants), VK_SHADER_STAGE_VERTEX_BIT)
                .build();

      create_pipeline_layout();

      pipeline_ = create_pipeline()
                      .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                      .set_polygon_mode(VK_POLYGON_MODE_FILL)
                      .set_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                      .set_multisampling_none()
                      .disable_blending(3)
                      .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
                      .build_pipeline(gpu_->getDevice());
    }

    void DeferredPass::destroy() {
    }

    void DeferredPass::execute(VkCommandBuffer cmd) {
      begin_renderpass(cmd);

      const auto frame = current_frame_index;
      const auto& per_frame_buffers = repository_->get_buffer<PerFrameDataBuffers>();
      const auto& ibl_buffers = repository_->get_buffer<IblBuffers>();
      const auto& mesh_buffers = repository_->get_buffer<MeshBuffers>();

      vkCmdBindIndexBuffer(cmd, mesh_buffers.index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
      VkDescriptorSet descriptorSets[]
          = {per_frame_buffers.descriptor_sets[frame], ibl_buffers.descriptor_set,
             repository_->material_data.resource_set, repository_->material_data.constants_set,
             mesh_buffers.descriptor_set};
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 5,
                              descriptorSets, 0, nullptr);
      vkCmdSetViewport(cmd, 0, 1, &viewport_);
      vkCmdSetScissor(cmd, 0, 1, &scissor_);

      for (auto& r : repository_->main_draw_context_.opaque_surfaces) {
        GpuDrawPushConstants push_constants;
        push_constants.worldMatrix = r.transform;
        push_constants.material_id = r.material;
        vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(GpuDrawPushConstants), &push_constants);
        vkCmdDrawIndexed(cmd, r.index_count, 1, r.first_index, 0, 0);
      }
      if (registry_->config_.always_opaque) {
        for (auto& r : repository_->main_draw_context_.transparent_surfaces) {
          GpuDrawPushConstants push_constants;
          push_constants.worldMatrix = r.transform;
          push_constants.material_id = r.material;
          vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                             sizeof(GpuDrawPushConstants), &push_constants);
          vkCmdDrawIndexed(cmd, r.index_count, 1, r.first_index, 0, 0);
        }
      }

      vkCmdEndRendering(cmd);
    }
    /*
    void MeshletPass::prepare() {
      fmt::print("preparing deferred pass\n");

      const auto& per_frame_buffers = repository_->get_buffer<PerFrameDataBuffers>();
      const auto& ibl_buffers = repository_->get_buffer<IblBuffers>();
      const auto& mesh_buffers = repository_->get_buffer<MeshBuffers>();
      descriptor_layouts_.push_back(per_frame_buffers.descriptor_layout);
      descriptor_layouts_.push_back(ibl_buffers.descriptor_layout);
      descriptor_layouts_.push_back(repository_->material_data.resource_layout);
      descriptor_layouts_.push_back(repository_->material_data.constants_layout);
      descriptor_layouts_.push_back(mesh_buffers.descriptor_layout);

      VkShaderModule fragShader;
      vkutil::load_shader_module(fragment_shader_source_.c_str(), gpu_->getDevice(), &fragShader);

      VkShaderModule meshShader;
      vkutil::load_shader_module(mesh_shader_source_.c_str(), gpu_->getDevice(), &meshShader);
      VkShaderModule taskShader;
      vkutil::load_shader_module(task_shader_source_.c_str(), gpu_->getDevice(), &taskShader);

      VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
      mesh_layout_info.setLayoutCount = descriptor_layouts_.size();
      mesh_layout_info.pSetLayouts = descriptor_layouts_.data();

      VK_CHECK(vkCreatePipelineLayout(gpu_->getDevice(), &mesh_layout_info, nullptr, &pipeline_layout_));

      const auto gbuffer_1 = registry_->get_resource<TextureHandle>("gbuffer_1");
      const auto gbuffer_2 = registry_->get_resource<TextureHandle>("gbuffer_2");
      const auto gbuffer_3 = registry_->get_resource<TextureHandle>("gbuffer_3");
      const auto depth_image = registry_->get_resource<TextureHandle>("gbuffer_d");

      pipeline_ = PipelineBuilder()
                      .set_shaders(taskShader, meshShader, fragShader)
                      .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                      .set_polygon_mode(VK_POLYGON_MODE_FILL)
                      .set_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                      .set_multisampling_none()
                      .disable_blending(3)
                      .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
                      .set_color_attachment_formats(
                          {gbuffer_1->getFormat(), gbuffer_2->getFormat(), gbuffer_3->getFormat()})
                      .set_depth_format(depth_image->getFormat())
                      .set_pipeline_layout(pipeline_layout_)
                      .build_pipeline(gpu_->getDevice());

      vkDestroyShaderModule(gpu_->getDevice(), taskShader, nullptr);
      vkDestroyShaderModule(gpu_->getDevice(), meshShader, nullptr);
      vkDestroyShaderModule(gpu_->getDevice(), fragShader, nullptr);
    }

    void MeshletPass::cleanup() { vkDestroyPipeline(gpu_->getDevice(), pipeline_, nullptr); }

    void MeshletPass::execute(VkCommandBuffer cmd) {
      const auto gbuffer_1 = registry_->get_resource<TextureHandle>("gbuffer_1");
      const auto gbuffer_2 = registry_->get_resource<TextureHandle>("gbuffer_2");
      const auto gbuffer_3 = registry_->get_resource<TextureHandle>("gbuffer_3");
      const auto depth_image = registry_->get_resource<TextureHandle>("gbuffer_d");

      VkClearValue depth_clear = {.depthStencil = {1.f, 0}};
      VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(
          depth_image->imageView, &depth_clear, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

      std::array attachment_begin_infos{
          vkinit::attachment_info(gbuffer_1->imageView, nullptr,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
          vkinit::attachment_info(gbuffer_2->imageView, nullptr,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
          vkinit::attachment_info(gbuffer_3->imageView, nullptr,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)};

      VkRenderingInfo renderInfo = vkinit::rendering_info_for_gbuffer(
          gbuffer_1->getExtent2D(), attachment_begin_infos.data(), attachment_begin_infos.size(),
          &depthAttachment);

      vkCmdBeginRendering(cmd, &renderInfo);

      const char frameIndex = gpu_.get_current_frame();
      auto& per_frame_buffers = repository_->get_buffer<PerFrameDataBuffers>();
      const auto& ibl_buffers = repository_->get_buffer<IblBuffers>();
      const auto& mesh_buffers = repository_->get_buffer<MeshBuffers>();

      VkDescriptorBufferInfo buffer_info;
      buffer_info.buffer = per_frame_buffers.uniform_buffers[frameIndex].buffer;
      buffer_info.offset = 0;
      buffer_info.range = sizeof(PerFrameData);

      VkWriteDescriptorSet descriptor_write = {};
      descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptor_write.dstBinding = 0;
      descriptor_write.dstArrayElement = 0;
      descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      descriptor_write.descriptorCount = 1;
      descriptor_write.pBufferInfo = &buffer_info;

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
      VkDescriptorSet descriptorSets[]
          = {ibl_buffers.descriptor_set, repository_->material_data.resource_set,
             repository_->material_data.constants_set,
             mesh_buffers.descriptor_set};

      gpu_.vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                                     &descriptor_write);
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 1, 4,
                              descriptorSets, 0, nullptr);

      viewport_.width = static_cast<float>(depth_image->getExtent2D().width);
      viewport_.height = static_cast<float>(depth_image->getExtent2D().height);
      scissor_.extent = depth_image->getExtent2D();
      vkCmdSetViewport(cmd, 0, 1, &viewport_);
      vkCmdSetScissor(cmd, 0, 1, &scissor_);

      // wip
      size_t maxCount = repository_->main_draw_context_.opaque_surfaces.size();

      AllocatedBuffer countBuffer;
      AllocatedBuffer argumentBuffer;

      gpu_.vkCmdDrawMeshTasksIndirectCountEXT(cmd, argumentBuffer.buffer,
                                              argumentBuffer.info.offset, countBuffer.buffer,
                                              countBuffer.info.offset, maxCount, 0);

      vkCmdEndRendering(cmd);
    }

    void TransparentPass::prepare() {
      fmt::print("preparing transparent pass\n");

      const auto& per_frame_buffers = repository_->get_buffer<PerFrameDataBuffers>();
      const auto& ibl_buffers = repository_->get_buffer<IblBuffers>();
      descriptor_layouts_.push_back(per_frame_buffers.descriptor_layout);
      descriptor_layouts_.push_back(ibl_buffers.descriptor_layout);
      descriptor_layouts_.push_back(repository_->material_data.resource_layout);
      descriptor_layouts_.push_back(repository_->material_data.constants_layout);
      descriptor_layouts_.emplace_back(DescriptorLayoutBuilder()
                                           .add_binding(10,
                                                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                        VK_SHADER_STAGE_FRAGMENT_BIT)
                                           .build(gpu_->getDevice()));

      VkShaderModule meshFragShader;
      vkutil::load_shader_module(fragment_shader_source_.c_str(), gpu_->getDevice(), &meshFragShader);

      VkShaderModule meshVertexShader;
      vkutil::load_shader_module(vertex_shader_source_.c_str(), gpu_->getDevice(), &meshVertexShader);

      VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
      mesh_layout_info.setLayoutCount = descriptor_layouts_.size();
      mesh_layout_info.pSetLayouts = descriptor_layouts_.data();
      mesh_layout_info.pPushConstantRanges = &push_constant_range_;
      mesh_layout_info.pushConstantRangeCount = 1;

      VK_CHECK(vkCreatePipelineLayout(gpu_->getDevice(), &mesh_layout_info, nullptr, &pipeline_layout_));

      const auto color_image = registry_->get_resource<TextureHandle>("scene_opaque_color");
      const auto depth_image = registry_->get_resource<TextureHandle>("scene_opaque_depth");

      pipeline_ = PipelineBuilder()
                      .set_shaders(meshVertexShader, meshFragShader)
                      .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                      .set_polygon_mode(VK_POLYGON_MODE_FILL)
                      .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                      .set_multisampling_none()
                      .enable_blending_additive()
                      .enable_depthtest(false, VK_COMPARE_OP_LESS_OR_EQUAL)
                      .set_color_attachment_format(color_image->getFormat())
                      .set_depth_format(depth_image->getFormat())
                      .set_pipeline_layout(pipeline_layout_)
                      .build_pipeline(gpu_->getDevice());

      vkDestroyShaderModule(gpu_->getDevice(), meshFragShader, nullptr);
      vkDestroyShaderModule(gpu_->getDevice(), meshVertexShader, nullptr);
    }

    void TransparentPass::execute(VkCommandBuffer cmd) {
      descriptor_set_
          = resource_manager_->descriptor_pool->allocate(gpu_->getDevice(), descriptor_layouts_.at(4));

      const auto color_image = registry_->get_resource<TextureHandle>("scene_opaque_color");
      const auto depth_image = registry_->get_resource<TextureHandle>("scene_opaque_depth");

      const auto shadow_map = registry_->get_resource<TextureHandle>("directional_shadow_map");

      VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(
          color_image->imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(
          depth_image->imageView, nullptr, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

      VkRenderingInfo renderInfo
          = vkinit::rendering_info(color_image->getExtent2D(), &colorAttachment, &depthAttachment);

      vkCmdBeginRendering(cmd, &renderInfo);

      const char frameIndex = gpu_.get_current_frame();
      const auto& per_frame_buffers = repository_->get_buffer<PerFrameDataBuffers>();
      const auto& mesh_buffers = repository_->get_buffer<MeshBuffers>();
      const auto& ibl_buffers = repository_->get_buffer<IblBuffers>();

      VkDescriptorBufferInfo buffer_info;
      buffer_info.buffer = per_frame_buffers.uniform_buffers[frameIndex].buffer;
      buffer_info.offset = 0;
      buffer_info.range = sizeof(PerFrameData);

      VkWriteDescriptorSet descriptor_write = {};
      descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptor_write.dstBinding = 0;
      descriptor_write.dstArrayElement = 0;
      descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      descriptor_write.descriptorCount = 1;
      descriptor_write.pBufferInfo = &buffer_info;

      vkCmdBindIndexBuffer(cmd, mesh_buffers.index_buffer.buffer, 0,
                           VK_INDEX_TYPE_UINT32);

      VkDescriptorSet descriptorSets[]
          = {ibl_buffers.descriptor_set, repository_->material_data.resource_set,
             repository_->material_data.constants_set, descriptor_set_};

      viewport_.width = static_cast<float>(depth_image->getExtent2D().width);
      viewport_.height = static_cast<float>(depth_image->getExtent2D().height);
      scissor_.extent = depth_image->getExtent2D();
      vkCmdSetViewport(cmd, 0, 1, &viewport_);
      vkCmdSetScissor(cmd, 0, 1, &scissor_);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
      writer.clear();
      writer.write_image(10, shadow_map->imageView, repository_->default_material_.nearestSampler,
                         VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.update_set(gpu_->getDevice(), descriptor_set_);

      gpu_.vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                                     &descriptor_write);
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 1, 4,
                              descriptorSets, 0, nullptr);

      for (auto& r : repository_->main_draw_context_.transparent_surfaces) {
        GpuDrawPushConstants push_constants;
        push_constants.worldMatrix = r.transform;
        push_constants.material_id = r.material;
        vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(GpuDrawPushConstants), &push_constants);
        vkCmdDrawIndexed(cmd, r.index_count, 1, r.first_index, 0, 0);
      }

      vkCmdEndRendering(cmd);
    }

    void TransparentPass::cleanup() { vkDestroyPipeline(gpu_->getDevice(), pipeline_, nullptr); }

    void DebugAabbPass::prepare() {
      fmt::print("preparing debug_aabb pass\n");

      const auto& per_frame_buffers = repository_->get_buffer<PerFrameDataBuffers>();
      descriptor_layouts_.push_back(per_frame_buffers.descriptor_layout);

      VkShaderModule meshFragShader;
      vkutil::load_shader_module(fragment_shader_source_.c_str(), gpu_->getDevice(), &meshFragShader);

      VkShaderModule meshVertexShader;
      vkutil::load_shader_module(vertex_shader_source_.c_str(), gpu_->getDevice(), &meshVertexShader);

      VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
      mesh_layout_info.setLayoutCount = descriptor_layouts_.size();
      mesh_layout_info.pSetLayouts = descriptor_layouts_.data();
      mesh_layout_info.pPushConstantRanges = &push_constant_range_;
      mesh_layout_info.pushConstantRangeCount = 1;

      VK_CHECK(vkCreatePipelineLayout(gpu_->getDevice(), &mesh_layout_info, nullptr, &pipeline_layout_));

      const auto color_image = registry_->get_resource<TextureHandle>("scene_final");
      const auto depth_image = registry_->get_resource<TextureHandle>("grid_depth");

      pipeline_ = PipelineBuilder()
                      .set_shaders(meshVertexShader, meshFragShader)
                      .set_input_topology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
                      .set_polygon_mode(VK_POLYGON_MODE_LINE)
                      .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                      .set_multisampling_none()
                      .disable_blending()
                      .enable_depthtest(false, VK_COMPARE_OP_LESS_OR_EQUAL)
                      .set_color_attachment_format(color_image->getFormat())
                      .set_depth_format(depth_image->getFormat())
                      .set_pipeline_layout(pipeline_layout_)
                      .build_pipeline(gpu_->getDevice());

      vkDestroyShaderModule(gpu_->getDevice(), meshFragShader, nullptr);
      vkDestroyShaderModule(gpu_->getDevice(), meshVertexShader, nullptr);
    }

    void DebugAabbPass::cleanup() {}

    void DebugAabbPass::execute(VkCommandBuffer cmd) {
      const auto color_image = registry_->get_resource<TextureHandle>("scene_final");
      const auto depth_image = registry_->get_resource<TextureHandle>("grid_depth");

      VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(
          color_image->imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(
          depth_image->imageView, nullptr, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

      VkRenderingInfo renderInfo
          = vkinit::rendering_info(color_image->getExtent2D(), &colorAttachment, &depthAttachment);

      const char frameIndex = gpu_.get_current_frame();
      const auto& per_frame_buffers = repository_->get_buffer<PerFrameDataBuffers>();

      VkDescriptorBufferInfo buffer_info;
      buffer_info.buffer = per_frame_buffers.uniform_buffers[frameIndex].buffer;
      buffer_info.offset = 0;
      buffer_info.range = sizeof(PerFrameData);

      VkWriteDescriptorSet descriptor_write = {};
      descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptor_write.dstBinding = 0;
      descriptor_write.dstArrayElement = 0;
      descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      descriptor_write.descriptorCount = 1;
      descriptor_write.pBufferInfo = &buffer_info;

      vkCmdBeginRendering(cmd, &renderInfo);

      viewport_.width = static_cast<float>(color_image->getExtent2D().width);
      viewport_.height = static_cast<float>(color_image->getExtent2D().height);
      scissor_.extent = color_image->getExtent2D();
      vkCmdSetViewport(cmd, 0, 1, &viewport_);
      vkCmdSetScissor(cmd, 0, 1, &scissor_);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

      gpu_.vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                                     &descriptor_write);
      if (registry_->config_.debug_aabb) {
        for (auto& node : repository_->scene_graph.components() | std::views::values) {
          if (!node.visible) continue;

          aabb_debug_push_constants push_constant{
              .min = glm::vec4(node.bounds.min, 1.0),
              .max = glm::vec4(node.bounds.max, 1.0),
          };
          vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                             sizeof(aabb_debug_push_constants), &push_constant);
          vkCmdDraw(cmd, 36, 1, 0, 0);
        }
      }

      vkCmdEndRendering(cmd);
    }*/
}  // namespace gestalt