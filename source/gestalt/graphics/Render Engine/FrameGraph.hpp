#pragma once
#include <memory>
#include <ranges>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common.hpp"
#include "Resources/ResourceTypes.hpp"
#include <algorithm>

#include "ResourceRegistry.hpp"
#include "Renderpasses/RenderPass.hpp"
#include "Utils/CommandBuffer.hpp"
#include "Utils/Pipeline.hpp"
#include "SynchronizationManager.hpp"

namespace gestalt::graphics {
  class ResourceAllocator;
}

namespace gestalt::graphics {
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


  class FrameGraph : public NonCopyable<FrameGraph> {
    std::vector<std::shared_ptr<FrameGraphNode>> nodes_;
    std::unordered_map<uint64, std::shared_ptr<FrameGraphEdge>> edges_;

    std::vector<std::shared_ptr<FrameGraphNode>> sorted_nodes_;

    std::vector<VkDescriptorBufferBindingInfoEXT> descriptor_buffer_bindings_;

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
                                                    = CreationType::INTERNAL);

    std::shared_ptr<BufferInstance> add_resource(BufferTemplate&& buffer_template,
                                                 CreationType creation_type
                                                 = CreationType::INTERNAL);


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