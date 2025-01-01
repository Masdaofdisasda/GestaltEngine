#include "SynchronizationManager.hpp"
#include "FrameGraph.hpp"

namespace gestalt::graphics {
  VkPipelineStageFlags2 SynchronizationManager::SynchronizationVisitor::get_dst_stage_mask(
      const VkShaderStageFlags shader_stage) {
    VkPipelineStageFlags2 dst_stage_mask = 0;

    if (shader_stage & VK_SHADER_STAGE_VERTEX_BIT) {
      dst_stage_mask |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    }

    if (shader_stage & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) {
      dst_stage_mask |= VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT;
    }

    if (shader_stage & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) {
      dst_stage_mask |= VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT;
    }

    if (shader_stage & VK_SHADER_STAGE_GEOMETRY_BIT) {
      dst_stage_mask |= VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT;
    }

    if (shader_stage & VK_SHADER_STAGE_FRAGMENT_BIT) {
      dst_stage_mask |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    }

    if (shader_stage & VK_SHADER_STAGE_COMPUTE_BIT) {
      dst_stage_mask |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    }

    if (shader_stage & VK_SHADER_STAGE_TASK_BIT_EXT) {
      dst_stage_mask |= VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT;
    }

    if (shader_stage & VK_SHADER_STAGE_MESH_BIT_EXT) {
      dst_stage_mask |= VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT;
    }

    if (shader_stage & VK_SHADER_STAGE_RAYGEN_BIT_KHR) {
      dst_stage_mask |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    }

    if (shader_stage & VK_SHADER_STAGE_ANY_HIT_BIT_KHR) {
      dst_stage_mask |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    }

    if (shader_stage & VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR) {
      dst_stage_mask |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    }

    if (shader_stage & VK_SHADER_STAGE_MISS_BIT_KHR) {
      dst_stage_mask |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    }

    if (shader_stage & VK_SHADER_STAGE_INTERSECTION_BIT_KHR) {
      dst_stage_mask |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    }

    if (shader_stage & VK_SHADER_STAGE_CALLABLE_BIT_KHR) {
      dst_stage_mask |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    }

    // As a fallback
    if (dst_stage_mask == 0) {
      dst_stage_mask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    }

    return dst_stage_mask;

  }

  void SynchronizationManager::SynchronizationVisitor::visit(BufferInstance& buffer,
      const ResourceUsage usage, const VkShaderStageFlags shader_stage) {

    VkBufferMemoryBarrier2 barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    barrier.buffer = buffer.get_buffer_handle();
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;

    barrier.srcAccessMask = buffer.get_current_access();
    barrier.srcStageMask = buffer.get_current_stage();

    const VkPipelineStageFlags2 dst_stage_mask = get_dst_stage_mask(shader_stage);
    VkAccessFlags2 dst_access_mask = 0;

    if (usage == ResourceUsage::WRITE) {
      // Writing to a storage buffer
      dst_access_mask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
    } else if (usage == ResourceUsage::READ) {
      // Reading from a uniform or storage buffer
      // If uniform: VK_ACCESS_2_UNIFORM_READ_BIT
      // If storage: VK_ACCESS_2_SHADER_STORAGE_READ_BIT
      // For a generic read assumption:
      dst_access_mask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_UNIFORM_READ_BIT;
    }

    // Update the barrier
    barrier.dstAccessMask = dst_access_mask;
    barrier.dstStageMask = dst_stage_mask;

    // Update the resource state
    buffer.set_current_access(dst_access_mask);
    buffer.set_current_stage(dst_stage_mask);

    buffer_barriers.push_back(barrier);
  }

  void SynchronizationManager::SynchronizationVisitor::visit(ImageInstance& image,
      ResourceUsage usage, const VkShaderStageFlags shader_stage) {
    VkImageMemoryBarrier2 barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.image = image.get_image_handle();
    barrier.subresourceRange = vkinit::image_subresource_range(image.get_image_aspect());

    // Source state
    barrier.oldLayout = image.get_layout();
    barrier.srcAccessMask = image.get_current_access();
    barrier.srcStageMask = image.get_current_stage();

    // Determine pipeline stages
    VkPipelineStageFlags2 dstStageMask = 0;
    if (shader_stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
      dstStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
    } else if (shader_stage == VK_SHADER_STAGE_COMPUTE_BIT) {
      dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    } else {
      dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    }

    // Determine new layout and access based on usage and texture type
    VkImageLayout newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkAccessFlags2 dstAccessMask = 0;
    switch (image.get_type()) {
      case TextureType::kColor:
        if (shader_stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
          if (usage == ResourceUsage::READ) {
            // Sampled read
            newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
          } else {
            // Color attachment write
            newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
          }
        } else {
          // Compute stage
          if (usage == ResourceUsage::READ) {
            newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
          } else {
            // General layout for compute read/write (e.g. storage image)
            newLayout = VK_IMAGE_LAYOUT_GENERAL;
            dstAccessMask
                = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
          }
        }
        break;

      case TextureType::kDepth:
        if (shader_stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
          if (usage == ResourceUsage::READ) {
            newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
          } else {
            newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
                            | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
          }
        } else {
          // For compute reading a depth texture, typically sampled read:
          if (usage == ResourceUsage::READ) {
            newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
          } else {
            // General for compute write to depth? Usually unusual, but handle generically:
            newLayout = VK_IMAGE_LAYOUT_GENERAL;
            dstAccessMask
                = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
          }
        }
        break;

      default:
        throw std::runtime_error("Unsupported TextureType for image!");
    }

    barrier.newLayout = newLayout;
    barrier.dstAccessMask = dstAccessMask;
    barrier.dstStageMask = dstStageMask;

    // Update image state
    image.set_layout(newLayout);
    image.set_current_access(dstAccessMask);
    image.set_current_stage(dstStageMask);

    image_barriers.push_back(barrier);
  }

  void SynchronizationManager::SynchronizationVisitor::visit(ImageArrayInstance& images,
      ResourceUsage usage, VkShaderStageFlags shader_stage) {
    for (auto& image : images.get_materials()) {
      //TODO
      //image->accept(*this, usage, shader_stage);
    }
  }

  void SynchronizationManager::SynchronizationVisitor::apply() const {
    VkDependencyInfo dependency_info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependency_info.dependencyFlags = 0;
    dependency_info.bufferMemoryBarrierCount = static_cast<uint32_t>(buffer_barriers.size());
    dependency_info.pBufferMemoryBarriers = buffer_barriers.data();
    dependency_info.imageMemoryBarrierCount = static_cast<uint32_t>(image_barriers.size());
    dependency_info.pImageMemoryBarriers = image_barriers.data();
    dependency_info.memoryBarrierCount = 0;
    dependency_info.pMemoryBarriers = nullptr;

    vkCmdPipelineBarrier2(cmd_.get(), &dependency_info);
  }

  void SynchronizationManager::synchronize_resources(const std::shared_ptr<FrameGraphNode>& node,
      const CommandBuffer cmd) {
    const auto read_resources = node->render_pass->get_resources(ResourceUsage::READ);
    const auto write_resources = node->render_pass->get_resources(ResourceUsage::WRITE);
      
    SynchronizationVisitor visitor(cmd);

    for (const auto& [resource, info, _] : read_resources) {
      resource->accept(visitor, ResourceUsage::READ,
                       info.shader_stages);
    }

    for (const auto& [resource, info, _] : write_resources) {
      resource->accept(visitor, ResourceUsage::WRITE, info.shader_stages);
    }

    visitor.apply();
  }
}  // namespace gestalt::graphics::fg