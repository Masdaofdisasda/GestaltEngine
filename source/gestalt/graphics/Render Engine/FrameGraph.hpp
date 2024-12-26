#pragma once
#include <array>
#include <bitset>
#include <memory>
#include <ranges>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ResourceTypes.hpp"
#include "common.hpp"
#include "vk_initializers.hpp"
#include "Interface/IGpu.hpp"
#include "Resources/ResourceTypes.hpp"
#include <algorithm>

#include "Renderpasses/RenderPass.hpp"
#include "Utils/CommandBuffer.hpp"
#include "Utils/Pipeline.hpp"

namespace gestalt::graphics {
  class ResourceAllocator;
}

namespace gestalt::graphics::fg {
  struct FrameGraphNode;

  // defines if the resource is used internal or external
  // internal resources are created and managed by the frame graph
  // external resources are created and managed by some other system
  // external resources may not count as dependencies for the frame graph
  enum class CreationType { INTERNAL, EXTERNAL };

  struct FrameGraphEdge {
    std::shared_ptr<ResourceInstance> resource;
    std::vector<std::shared_ptr<FrameGraphNode>> nodes_from;
    std::vector<std::shared_ptr<FrameGraphNode>> nodes_to;
    CreationType creation_type;

    explicit FrameGraphEdge(const std::shared_ptr<ResourceInstance>& resource,
                            const CreationType creation_type)
        : resource(resource), creation_type(creation_type) {}
  };

  struct FrameGraphNode {
    std::shared_ptr<RenderPass> render_pass;
    std::vector<std::shared_ptr<FrameGraphEdge>> edges_in;
    std::vector<std::shared_ptr<FrameGraphEdge>> edges_out;

    explicit FrameGraphNode(std::shared_ptr<RenderPass>&& render_pass)
        : render_pass(std::move(render_pass)) {}

    [[nodiscard]] std::vector<std::shared_ptr<FrameGraphNode>> get_successors() const {
      std::vector<std::shared_ptr<FrameGraphNode>> neighbors;
      for (const auto& edge : edges_out) {
        for (const auto& neighbor : edge->nodes_to) {
          neighbors.push_back(neighbor);
        }
      }
      return neighbors;
    }
  };

  class SynchronizationManager {
    class SynchronizationVisitor final : public ResourceVisitor {
      CommandBuffer cmd_;
      std::vector<VkImageMemoryBarrier2> image_barriers;
      std::vector<VkBufferMemoryBarrier2> buffer_barriers;

    public:
      explicit SynchronizationVisitor(const CommandBuffer cmd) : cmd_(cmd) {}

      void visit(BufferInstance& buffer, const ResourceUsage usage,
                 const VkShaderStageFlags shader_stage) override {
        // Prepare a buffer memory barrier
        VkBufferMemoryBarrier2 barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
        barrier.buffer = buffer.get_buffer_handle();
        barrier.offset = 0;
        barrier.size = VK_WHOLE_SIZE;

        // Source: from the buffer's current known state
        barrier.srcAccessMask = buffer.get_current_access();
        barrier.srcStageMask = buffer.get_current_stage();

        // Determine destination stages and access masks based on usage
        // If we know the exact shader stage, we could pick a more granular pipeline stage:
        VkPipelineStageFlags2 dstStageMask = 0;
        VkAccessFlags2 dstAccessMask = 0;

        // Assign destination pipeline stages from shader_stage
        // This can be refined if you know whether it's fragment-only, vertex-only, etc.
        if (shader_stage == VK_SHADER_STAGE_ALL_GRAPHICS) {
          dstStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
        } else if (shader_stage == VK_SHADER_STAGE_COMPUTE_BIT) {
          dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        } else {
          // As a fallback for other shader stages:
          dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        }

        // Assign access mask based on usage
        // Refine this depending on whether the buffer is used as a uniform, storage buffer, etc.
        // For general READ: assume uniform buffer or sampled read.
        // For WRITE: assume storage buffer writes.
        if (usage == ResourceUsage::WRITE) {
          // Writing to a storage buffer
          dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
        } else if (usage == ResourceUsage::READ) {
          // Reading from a uniform or storage buffer
          // If uniform: VK_ACCESS_2_UNIFORM_READ_BIT
          // If storage: VK_ACCESS_2_SHADER_STORAGE_READ_BIT
          // For a generic read assumption:
          dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
        }

        // Update the barrier
        barrier.dstAccessMask = dstAccessMask;
        barrier.dstStageMask = dstStageMask;

        // Update the resource state
        buffer.set_current_access(dstAccessMask);
        buffer.set_current_stage(dstStageMask);

        buffer_barriers.push_back(barrier);
      }

      void visit(ImageInstance& image, ResourceUsage usage,
                 VkShaderStageFlags shader_stage) override {
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

      void visit(ImageArrayInstance& images, ResourceUsage usage,
                 VkShaderStageFlags shader_stage) override {
        for (auto& image : images.get_images()) {
          image->accept(*this, usage, shader_stage);
        }
      }


      void apply() const {
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
    };

  public:
    void synchronize_resources(const std::shared_ptr<FrameGraphNode>& node,
                               const CommandBuffer cmd) {
      const auto read_resources = node->render_pass->get_resources(ResourceUsage::READ);
      const auto write_resources = node->render_pass->get_resources(ResourceUsage::WRITE);
      const auto bind_point = node->render_pass->get_bind_point();

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
  };

  class ResourceRegistry {
    std::unordered_map<uint64, std::shared_ptr<ResourceInstance>> resource_map_;
    ResourceAllocator* resource_factory_ = nullptr;

  public:
    explicit ResourceRegistry(ResourceAllocator* resource_factory)
        : resource_factory_(resource_factory) {}

    std::shared_ptr<ImageInstance> add_template(ImageTemplate&& image_template);

    std::shared_ptr<BufferInstance> add_template(BufferTemplate&& buffer_template);


    template <typename ResourceInstanceType> std::shared_ptr<ResourceInstanceType> add_resource(
        std::shared_ptr<ResourceInstanceType> resource_instance) {
      if (resource_instance == nullptr) {
        throw std::runtime_error("Resource instance cannot be null!");
      }
      resource_instance->set_handle(reinterpret_cast<uint64>(resource_instance.get()));

      resource_map_.insert({resource_instance->handle(), resource_instance});

      auto it = resource_map_.find(resource_instance->handle());
      if (it == resource_map_.end()) {
        throw std::runtime_error("Failed to insert resource into registry!");
      }

      return std::static_pointer_cast<ResourceInstanceType>(it->second);
    }

    std::shared_ptr<ResourceInstance> get_resource(const uint32 handle) {
      return resource_map_.at(handle);
    }
  };


  class FrameGraph : public NonCopyable<FrameGraph> {
    std::vector<std::shared_ptr<FrameGraphNode>> nodes_;
    std::unordered_map<uint64, std::shared_ptr<FrameGraphEdge>> edges_;

    std::vector<std::shared_ptr<FrameGraphNode>> sorted_nodes_;

    std::unique_ptr<SynchronizationManager> synchronization_manager_;
    std::unique_ptr<ResourceRegistry> resource_registry_;

    void add_render_pass(std::shared_ptr<RenderPass>&& pass);

    void print_graph() const;

    void topological_sort();

  public:
    explicit FrameGraph(ResourceAllocator* resource_allocator);

    template <typename PassType, typename... Args> void add_pass(Args&&... args) {
      add_render_pass(std::make_shared<PassType>(std::forward<Args>(args)...));
    }


    std::shared_ptr<ImageInstance> add_resource(ImageTemplate&& image_template,
                                                CreationType creation_type
                                                    = CreationType::INTERNAL) {
      auto resource = resource_registry_->add_template(std::move(image_template));
      const uint64 handle = resource->handle();
      if (handle == -1) {
        throw std::runtime_error("Invalid resource handle!");
      }

      auto inserted = edges_.emplace(
          handle, std::make_shared<FrameGraphEdge>(std::move(resource), creation_type));
      if (!inserted.second) {
        throw std::runtime_error("Failed to insert edge into edges map!");
      }

      return std::static_pointer_cast<ImageInstance>(inserted.first->second->resource);
    }

    std::shared_ptr<BufferInstance> add_resource(BufferTemplate&& buffer_template,
                                                 CreationType creation_type
                                                 = CreationType::INTERNAL) {
      auto resource = resource_registry_->add_template(std::move(buffer_template));
      const uint64 handle = resource->handle();
      if (handle == -1) {
        throw std::runtime_error("Invalid resource handle!");
      }

      auto inserted = edges_.emplace(
          handle, std::make_shared<FrameGraphEdge>(std::move(resource), creation_type));
      if (!inserted.second) {
        throw std::runtime_error("Failed to insert edge into edges map!");
      }

      return std::static_pointer_cast<BufferInstance>(inserted.first->second->resource);
    }


    template <typename ResourceInstanceType>
    auto add_resource(std::shared_ptr<ResourceInstanceType> resource_instance,
                      CreationType creation_type = CreationType::EXTERNAL) {
      if (resource_instance == nullptr) {
        throw std::runtime_error("Resource instance cannot be null!");
      }

      auto resource = resource_registry_->add_resource(resource_instance);
      if (resource == nullptr) {
        throw std::runtime_error("Failed to add resource to the registry!");
      }

      uint64 handle = resource->handle();
      if (handle == -1) {
        throw std::runtime_error("Invalid resource handle!");
      }

      auto inserted = edges_.emplace(
          handle, std::make_shared<FrameGraphEdge>(std::move(resource), creation_type));
      if (!inserted.second) {
        throw std::runtime_error("Failed to insert edge into edges map!");
      }

      return std::static_pointer_cast<ResourceInstanceType>(inserted.first->second->resource);
    }


    void compile();

    void execute(CommandBuffer cmd) const;
  }; 

}  // namespace gestalt::graphics::fg