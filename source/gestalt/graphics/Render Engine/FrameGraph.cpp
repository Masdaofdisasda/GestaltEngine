#include "Render Engine/FrameGraph.hpp"

#include "ResourceFactory.hpp"

namespace gestalt::graphics::fg {
  std::shared_ptr<Resource> ResourceRegistry::add(ImageResourceTemplate&& resource_template) {

    auto image = resource_factory_->create_image(resource_template);

    //std::shared_ptr<Resource> image = std::make_shared<ImageResource>(std::move(resource));
    image->handle = static_cast<uint32>(resource_map_.size());
    resource_map_.insert({image->handle, image});
    return image;
  }

  std::shared_ptr<Resource> ResourceRegistry::add(BufferResourceTemplate&& resource) {
    // create buffer

    std::shared_ptr<Resource> image = std::make_shared<BufferResource>(std::move(resource));
    image->handle = static_cast<uint32>(resource_map_.size());
    resource_map_.insert({image->handle, image});
    return image;
  }

  void FrameGraph::print_graph() const {
    for (const auto& node : nodes_) {
      fmt::println(" - Render Pass: {}", node->render_pass->name);
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
      assert(!"Cycle detected in the graph! Topological sorting is not possible.");
    }

    sorted_nodes_ = std::move(sorted_nodes);

    fmt::print("Sorted Nodes:\n");
    for (const auto& node : sorted_nodes_) {
      fmt::println("{}", node->render_pass->name);
    }
  }

  FrameGraph::FrameGraph(ResourceFactory* resource_factory) {
    nodes_.reserve(25);
    descriptor_manger_ = std::make_unique<DescriptorManger>();
    synchronization_manager_ = std::make_unique<SynchronizationManager>();
    resource_registry_ = std::make_unique<ResourceRegistry>(resource_factory);
  }

  void FrameGraph::add_render_pass(std::shared_ptr<RenderPass>&& pass) {
    nodes_.push_back(std::make_shared<FrameGraphNode>(std::move(pass)));
  }

  void FrameGraph::compile() {
    for (auto& node : nodes_) {
      for (const auto& read_resource : node->render_pass->get_resources(ResourceUsage::READ)) {
        auto& edge = edges_.at(read_resource->handle);
        edge->nodes_to.push_back(node);
        node->edges_in.push_back(edge);
      }
      for (const auto& write_resource : node->render_pass->get_resources(ResourceUsage::WRITE)) {
        auto& edge = edges_.at(write_resource->handle);
        edge->nodes_from.push_back(node);
        node->edges_out.push_back(edge);
      }
    }

    print_graph();
    topological_sort();
  }

  void FrameGraph::execute() {
    // run each render pass in order and synchronize resources
    // for (auto& pass_node : pass_nodes_) {
    // bind resources to descriptor sets
    // sync resources
    // pass_node.pass.execute(pass_node.inputs...)

    for (const auto& node : sorted_nodes_) {
      CommandBuffer cmd;
      node->render_pass->execute(cmd);
    }
  }
}  // namespace gestalt::graphics::fg