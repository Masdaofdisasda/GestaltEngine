#include "RenderPass.hpp"

#include "vk_images.hpp"
#include "vk_pipelines.hpp"

#include <fmt/core.h>

#include "FrameProvider.hpp"
#include "Interface/IGpu.hpp"

namespace gestalt::graphics {

  struct DrawCullConstants {
      int32 draw_count;
    };

    void DrawCullPass::prepare() {
	  fmt::print("Preparing {}\n", get_name());

          descriptor_layouts_.push_back(
              DescriptorLayoutBuilder()
                  .add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT
                                   | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_COMPUTE_BIT
                                   | VK_SHADER_STAGE_FRAGMENT_BIT)
                  .build(gpu_->getDevice()));

          constexpr auto mesh_stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT
                                       | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_COMPUTE_BIT;
          descriptor_layouts_.push_back(
              DescriptorLayoutBuilder()
                  .add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
                  .add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
                  .add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
                  .add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
                  .add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
                  .add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
                  .add_binding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
                  .add_binding(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
                  .build(gpu_->getDevice()));

	  dependencies_
		  = RenderPassDependencyBuilder()
                    .add_shader(ShaderStage::kCompute, "draw_cull.comp.spv")
                    .add_buffer_dependency(registry_->resources_.perFrameDataBuffer, BufferUsageType::kRead, 0)
                    .add_buffer_dependency(registry_->resources_.meshBuffer, BufferUsageType::kWrite, 1)
                    .set_push_constant_range(sizeof(DrawCullConstants), VK_SHADER_STAGE_COMPUTE_BIT)
				.build();

	  create_pipeline_layout();

	  pipeline_ = create_pipeline()
					  .build_compute_pipeline(gpu_->getDevice());
    }

    void DrawCullPass::destroy() {
    }

    void DrawCullPass::execute(VkCommandBuffer cmd) {
      const auto frame = frame_->get_current_frame_index();

      const auto& mesh_buffers = repository_->mesh_buffers;

      vkCmdFillBuffer(cmd, mesh_buffers->draw_count_buffer[frame]->buffer, 0,
                      mesh_buffers->draw_count_buffer[frame]->info.size, 0);

      const int32 maxCommandCount
          = repository_->mesh_draws.size();  // each mesh gets a draw command
      const uint32 groupCount
          = (static_cast<uint32>(maxCommandCount) + 63) / 64;  // 64 threads per group

      const DrawCullConstants draw_cull_constants{.draw_count = maxCommandCount};

      begin_compute_pass(cmd);

      vkCmdPushConstants(cmd, pipeline_layout_,
                         VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(DrawCullConstants), &draw_cull_constants);
      vkCmdDispatch(cmd, groupCount, 1, 1);

      // Memory barrier to ensure writes are visible to the second compute shader
      VkMemoryBarrier memoryBarrier = {};
      memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
      memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
      memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

      vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr,
                           0, nullptr);

    }

    void TaskSubmitPass::prepare() {
	  fmt::print("Preparing {}\n", get_name());

          
      constexpr auto mesh_stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT
                                       | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_COMPUTE_BIT;
          descriptor_layouts_.push_back(
              DescriptorLayoutBuilder()
                  .add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
                  .add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
                  .add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
                  .add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
                  .add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
                  .add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
                  .add_binding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
                  .add_binding(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
                  .build(gpu_->getDevice()));

	  dependencies_
		  = RenderPassDependencyBuilder()
                              .add_shader(ShaderStage::kCompute, "task_submit.comp.spv")
                              .add_buffer_dependency(registry_->resources_.meshBuffer,
                                                     BufferUsageType::kWrite, 0)
				.build();

	  create_pipeline_layout();

	  pipeline_ = create_pipeline()
					  .build_compute_pipeline(gpu_->getDevice());
    }

    void TaskSubmitPass::destroy() {
    }

    void TaskSubmitPass::execute(VkCommandBuffer cmd) {

        begin_compute_pass(cmd);
      vkCmdDispatch(cmd, 1, 1, 1);

      // Memory barrier to ensure writes are visible to the second compute shader
      VkMemoryBarrier memoryBarrier = {};
      memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
      memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
      memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

      vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
          VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT | VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT, 0, 1,
          &memoryBarrier, 0, nullptr,
                           0, nullptr);
    }

    void MeshletPass::prepare() {
      fmt::print("Preparing {}\n", get_name());

      descriptor_layouts_.push_back(
          DescriptorLayoutBuilder()
              .add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT
                               | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_COMPUTE_BIT
                               | VK_SHADER_STAGE_FRAGMENT_BIT)
              .build(gpu_->getDevice()));
      descriptor_layouts_.push_back(
          DescriptorLayoutBuilder()
              .add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_FRAGMENT_BIT)
              .add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_FRAGMENT_BIT)
              .add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_FRAGMENT_BIT)
              .add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_FRAGMENT_BIT, false, getMaxTextures())
              .add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
              .build(gpu_->getDevice()));
      constexpr auto mesh_stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT
                                   | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_COMPUTE_BIT;
      descriptor_layouts_.push_back(
          DescriptorLayoutBuilder()
              .add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
              .add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
              .add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
              .add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
              .add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
              .add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
              .add_binding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
              .add_binding(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
              .build(gpu_->getDevice()));

      dependencies_
          = RenderPassDependencyBuilder()
                .add_shader(ShaderStage::kTask, "geometry.task.spv")
                .add_shader(ShaderStage::kMesh, "geometry.mesh.spv")
                .add_shader(ShaderStage::kFragment, "geometry_deferred.frag.spv")
                .add_image_attachment(registry_->resources_.gbuffer1, ImageUsageType::kWrite, 0,
                                      ImageClearOperation::kClear)
                .add_image_attachment(registry_->resources_.gbuffer2, ImageUsageType::kWrite, 0,
                                      ImageClearOperation::kClear)
                .add_image_attachment(registry_->resources_.gbuffer3, ImageUsageType::kWrite, 0,
                                      ImageClearOperation::kClear)
                .add_image_attachment(registry_->resources_.scene_depth, ImageUsageType::kWrite, 0,
                                      ImageClearOperation::kClear)
                .add_buffer_dependency(registry_->resources_.perFrameDataBuffer,
                                       BufferUsageType::kRead, 0)
                .add_buffer_dependency(registry_->resources_.materialBuffer, BufferUsageType::kRead, 1)
                .add_buffer_dependency(registry_->resources_.meshBuffer, BufferUsageType::kRead, 2)
                .set_push_constant_range(
                    sizeof(MeshletPushConstants),
                    VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT)
                .build();

      create_pipeline_layout();

      pipeline_ = create_pipeline()
                      .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                      .set_polygon_mode(VK_POLYGON_MODE_FILL)
                      .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                      .set_multisampling_none()
                      .disable_blending(3)
                      .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
                      .build_graphics_pipeline(gpu_->getDevice());
    }

    void MeshletPass::destroy() {  }

    void MeshletPass::execute(VkCommandBuffer cmd) {
      begin_renderpass(cmd);

      const auto frame = frame_->get_current_frame_index();
      const auto& mesh_buffers = repository_->mesh_buffers;

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
      vkCmdSetViewport(cmd, 0, 1, &viewport_);
      vkCmdSetScissor(cmd, 0, 1, &scissor_);

      vkCmdPushConstants(cmd, pipeline_layout_,
                         VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT, 0,
                         sizeof(MeshletPushConstants), &meshlet_push_constants);

      // first byte is the task count, so we need offset by one uint32
      vkCmdDrawMeshTasksIndirectEXT(cmd, mesh_buffers->draw_count_buffer[frame]->buffer,
                                    sizeof(uint32), 1, 0);

      vkCmdEndRendering(cmd);
    }
}  // namespace gestalt