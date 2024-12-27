#pragma once 
#include <VulkanTypes.hpp>

#include "common.hpp"

namespace gestalt::graphics {
  class CommandBuffer {
    VkCommandBuffer cmd;

  public:
    explicit CommandBuffer(const VkCommandBuffer cmd) : cmd(cmd) {}
    // todo : add more command buffer functions and remove the get function
    [[nodiscard]] VkCommandBuffer get() const { return cmd; }

    void begin_rendering(const VkRenderingInfo& render_pass_info) const {
      vkCmdBeginRendering(cmd, &render_pass_info);
    }

    void end_rendering() const { vkCmdEndRendering(cmd); }

    void bind_pipeline(const VkPipelineBindPoint bind_point, const VkPipeline pipeline) const {
      vkCmdBindPipeline(cmd, bind_point, pipeline);
    }

    void pipeline_barrier(const VkPipelineStageFlags src_stage_mask,
                          const VkPipelineStageFlags dst_stage_mask,
                          const VkDependencyFlags dependency_flags,
                          const uint32 memory_barrier_count, const VkMemoryBarrier* memory_barriers,
                          const uint32 buffer_memory_barrier_count,
                          const VkBufferMemoryBarrier* buffer_memory_barriers,
                          const uint32 image_memory_barrier_count,
                          const VkImageMemoryBarrier* image_memory_barriers) const {
      vkCmdPipelineBarrier(cmd, src_stage_mask, dst_stage_mask, dependency_flags,
                           memory_barrier_count, memory_barriers, buffer_memory_barrier_count,
                           buffer_memory_barriers, image_memory_barrier_count,
                           image_memory_barriers);
    }

    void pipeline_barrier2(const VkDependencyInfo& dependency_info) const {
      vkCmdPipelineBarrier2(cmd, &dependency_info);
    }

    // only use for debugging
    void global_barrier() const {
      VkMemoryBarrier2 memoryBarrier = {
          .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
          .pNext = nullptr,
          .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
          .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
          .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
      };

      VkDependencyInfo dependencyInfo = {
          .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
          .pNext = nullptr,
          .dependencyFlags = 0,
          .memoryBarrierCount = 1,
          .pMemoryBarriers = &memoryBarrier,
          .bufferMemoryBarrierCount = 0,
          .pBufferMemoryBarriers = nullptr,
          .imageMemoryBarrierCount = 0,
          .pImageMemoryBarriers = nullptr,
      };
      pipeline_barrier2(dependencyInfo);
    }

    void bind_descriptor_buffers_ext(const uint32_t buffer_count,
                                     const VkDescriptorBufferBindingInfoEXT* binding_infos) const {
      vkCmdBindDescriptorBuffersEXT(cmd, buffer_count, binding_infos);
    }

    void fill_buffer(const VkBuffer buffer, const VkDeviceSize offset, const VkDeviceSize size,
                     const uint32 data) const {
      vkCmdFillBuffer(cmd, buffer, offset, size, data);
    }

    void set_viewport(const uint32 first_viewport, const uint32 viewport_count,
                      const VkViewport* viewports) const {
      vkCmdSetViewport(cmd, first_viewport, viewport_count, viewports);
    }

    void set_scissor(const uint32 first_scissor, const uint32 scissor_count,
                     const VkRect2D* scissors) const {
      vkCmdSetScissor(cmd, first_scissor, scissor_count, scissors);
    }

    void push_constants(const VkPipelineLayout layout, const VkShaderStageFlags stage_flags,
                        const uint32 offset, const uint32 size, const void* data) const {
      vkCmdPushConstants(cmd, layout, stage_flags, offset, size, data);
    }

    void draw_mesh_tasks_indirect_ext(const VkBuffer buffer, const VkDeviceSize offset,
                                      const uint32 draw_count, const uint32 stride) const {
      vkCmdDrawMeshTasksIndirectEXT(cmd, buffer, offset, draw_count, stride);
    }

    void draw(const uint32 vertex_count, const uint32 instance_count, const uint32 first_vertex,
              const uint32 first_instance) const {
      vkCmdDraw(cmd, vertex_count, instance_count, first_vertex, first_instance);
    }

    void dispatch(const uint32 group_count_x, const uint32 group_count_y,
                  const uint32 group_count_z) const {
      vkCmdDispatch(cmd, group_count_x, group_count_y, group_count_z);
    }

    void begin_debug_utils_label_ext(const VkDebugUtilsLabelEXT& label_info) const {
      vkCmdBeginDebugUtilsLabelEXT(cmd, &label_info);
    }

    void end_debug_utils_label_ext() const { vkCmdEndDebugUtilsLabelEXT(cmd); }
  };
}  // namespace gestalt