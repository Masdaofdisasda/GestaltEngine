#include "Render Engine/FrameGraph.hpp"

#include "ResourceAllocator.hpp"

namespace gestalt::graphics::fg {
  std::shared_ptr<ImageInstance> ResourceRegistry::add_template(ImageTemplate&& image_template) {
    const auto image = resource_factory_->create_image(std::move(image_template));
    
    return add_resource(image);
  }

  std::shared_ptr<BufferInstance> ResourceRegistry::add_template(BufferTemplate&& buffer_template) {
    const auto buffer = resource_factory_->create_buffer(std::move(buffer_template));
    
    return add_resource(buffer);
  }


  void FrameGraph::print_graph() const {
    for (const auto& node : nodes_) {
      fmt::println(" - Render Pass: {}", node->render_pass->get_name());
      fmt::println("   |-needs:");
      for (const auto& edge : node->edges_in) {
        fmt::println("   |---->{}", edge->resource->name());
      }
      fmt::println("   |-updates:");
      for (const auto& edge : node->edges_out) {
        fmt::println("   |---->{}", edge->resource->name());
      }
    }
  }

  void FrameGraph::topological_sort() {
    std::unordered_map<std::shared_ptr<FrameGraphNode>, uint32_t> in_degree;
    for (auto& node : nodes_) {
      for (const auto& edge : node->edges_in) {
        if (edge->creation_type == CreationType::INTERNAL) {
          in_degree[node]++;
        }
      }
    }

    std::queue<std::shared_ptr<FrameGraphNode>> nodes_without_dependencies;
    for (const auto& node : nodes_) {
      if (in_degree[node] == 0) {
        nodes_without_dependencies.push(node);
      }
    }

    std::vector<std::shared_ptr<FrameGraphNode>> sorted_nodes;

    while (!nodes_without_dependencies.empty()) {
      auto current_node = nodes_without_dependencies.front();
      nodes_without_dependencies.pop();
      sorted_nodes.push_back(current_node);

      for (const auto& neighbor : current_node->get_successors()) {
        in_degree[neighbor] -= 1;
        if (in_degree[neighbor] == 0) {
          nodes_without_dependencies.push(neighbor);
        }
      }
    }

    if (sorted_nodes.size() != nodes_.size()) {
      throw std::runtime_error("Cycle detected in the graph! Topological sorting is not possible.");
    }

    sorted_nodes_ = std::move(sorted_nodes);

    fmt::print("Sorted Nodes:\n");
    for (const auto& node : sorted_nodes_) {
      fmt::println("{}", node->render_pass->get_name());
    }
  }

  FrameGraph::FrameGraph(ResourceAllocator* resource_allocator) {
    nodes_.reserve(25);
    synchronization_manager_ = std::make_unique<SynchronizationManager>();
    resource_registry_ = std::make_unique<ResourceRegistry>(resource_allocator);
  }

  void FrameGraph::add_render_pass(std::shared_ptr<RenderPass>&& pass) {
    nodes_.push_back(std::make_shared<FrameGraphNode>(std::move(pass)));
  }

  void FrameGraph::compile() {
    for (auto& node : nodes_) {
      for (const auto& [read_resource, info, _] : node->render_pass->get_resources(ResourceUsage::READ)) {
        auto& edge = edges_.at(read_resource->handle());
        edge->nodes_to.push_back(node);
        node->edges_in.push_back(edge);
      }
      for (const auto& [write_resource, info, _]: node->render_pass->get_resources(ResourceUsage::WRITE)) {
        auto& edge = edges_.at(write_resource->handle());
        edge->nodes_from.push_back(node);
        node->edges_out.push_back(edge);
      }
    }

    print_graph();
    topological_sort();
  }

  void FrameGraph::execute(const CommandBuffer cmd) const {
    for (const auto& node : sorted_nodes_) {
      const auto name = std::string(node->render_pass->get_name());
      //fmt::println("executing: {}", name);
      synchronization_manager_->synchronize_resources(node, cmd);

      VkDebugUtilsLabelEXT label_info = {
          .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
          .pLabelName = name.c_str(),
          .color = {0.0f, 1.0f, 0.0f, 1.0f},
      };

      cmd.begin_debug_utils_label_ext(label_info);
      node->render_pass->execute(cmd);
      cmd.end_debug_utils_label_ext();
    }
  }
}  // namespace gestalt::graphics::fg