#include "Render Engine/FrameGraph.hpp"

#include "ResourceAllocator.hpp"
#include "SynchronizationManager.hpp"
#include "ResourceRegistry.hpp"

namespace gestalt::graphics::fg {

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

    // for debugging:
    //print_graph();

    topological_sort();

    for (const auto& node : sorted_nodes_) {
      for (const auto& [set, descriptor_buffer] : node->render_pass->get_descriptor_buffers()) {
        descriptor_buffer_bindings_.push_back(
            {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
             .address = descriptor_buffer->get_address(),
             .usage = descriptor_buffer->get_usage()});
      }
    }
  }

  void FrameGraph::execute(const CommandBuffer cmd) const {

    // TODO bind one global descriptor buffer
    //cmd.bind_descriptor_buffers_ext(static_cast<uint32>(descriptor_buffer_bindings_.size()), descriptor_buffer_bindings_.data());

    VkMemoryBarrier2 memory_barrier = {};
    memory_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    memory_barrier.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
    memory_barrier.srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT;
    memory_barrier.dstStageMask
        = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT
          | VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
          | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    memory_barrier.dstAccessMask
        = VK_ACCESS_2_UNIFORM_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT;

    VkDependencyInfo dependency_info = {};
    dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency_info.memoryBarrierCount = 1;
    dependency_info.pMemoryBarriers = &memory_barrier;

    cmd.pipeline_barrier2(dependency_info);


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
      cmd.global_barrier(); //TODO remove when 2 frames in flight is removed
      node->render_pass->execute(cmd);
      cmd.global_barrier();
      cmd.end_debug_utils_label_ext();
    }

    // Ensure all shader stages reading buffers complete before host writes
    memory_barrier.srcStageMask
        = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT
          | VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
          | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    memory_barrier.srcAccessMask
        = VK_ACCESS_2_UNIFORM_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT;

    // Allow host write after GPU completes
    memory_barrier.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
    memory_barrier.dstAccessMask = VK_ACCESS_2_HOST_WRITE_BIT;

    dependency_info = {};
    dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency_info.memoryBarrierCount = 1;
    dependency_info.pMemoryBarriers = &memory_barrier;

    // Submit the barrier
    cmd.pipeline_barrier2(dependency_info);
  }
}  // namespace gestalt::graphics::fg