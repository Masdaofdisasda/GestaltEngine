#pragma once
#include <array>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

#include "common.hpp"
#include "fmt/core.h"

namespace gestalt::graphics::fg {

  //TODO split into template and instance
  struct Resource {
    uint32 handle = -1;
    std::string name;
    explicit Resource(std::string name) : name(std::move(name)) {}
  };

  struct ImageResource : Resource {
    using Resource::Resource; // Inherit constructor
    // vkImage image;
  };

  struct BufferResource : Resource {
    using Resource::Resource; // Inherit constructor
    // vkBuffer buffer;
  };

  struct PushDescriptor {
    // vkPushDescriptor push_descriptor;
  };

  struct Resources {
    std::vector<std::shared_ptr<Resource>> resources;
    PushDescriptor push_descriptor;
  };

  class CommandBuffer {
    // vkCommandBuffer command_buffer;
    // draw()
    // dispatch()
  };

  struct ShaderComponent {
    virtual ~ShaderComponent() = default;
  };

  // helper functions for graphics shader pipeline
  struct GraphicsShaderComponent : ShaderComponent {

  };

  // helper functions for compute shader pipeline
  struct ComputeShaderComponent : ShaderComponent {

  };

  enum class ResourceUsage { READ, WRITE };

  struct ResourceComponent {
    void addResource(const std::shared_ptr<Resource>& resource, ResourceUsage usage) {
      if (usage == ResourceUsage::READ) {
        read_resources.resources.push_back(resource);
      } else if (usage == ResourceUsage::WRITE) {
        write_resources.resources.push_back(resource);
      }
    }

    std::vector<std::shared_ptr<Resource>> get_resources(ResourceUsage usage) {
      if (usage == ResourceUsage::READ) {
        return read_resources.resources;
      }
      if (usage == ResourceUsage::WRITE) {
        return write_resources.resources;
      }
      return {};
    }

  private:
    Resources read_resources;
    Resources write_resources;
  };

  struct RenderPass {
    virtual void compile() = 0;
    virtual std::vector<std::shared_ptr<Resource>> get_resources(ResourceUsage usage) = 0;
    virtual void execute(CommandBuffer cmd) = 0;
    virtual ~RenderPass() = default;
    std::string name;
  };

  struct ShadowMapPass : RenderPass {
    ComputeShaderComponent shader_component;
    ResourceComponent resource_component;

    ShadowMapPass(const std::shared_ptr<Resource>& shadow_map,
                  const std::shared_ptr<Resource>& geometry_buffer) {
      resource_component.addResource(geometry_buffer, ResourceUsage::READ);
      resource_component.addResource(shadow_map, ResourceUsage::WRITE);
      name = "ShadowMapPass";
    }

    std::vector<std::shared_ptr<Resource>> get_resources(ResourceUsage usage) override {
      return resource_component.get_resources(usage);
    }

    void compile() override {
      // Specific pipeline layout and pipeline creation for compute
    }

    void execute(CommandBuffer cmd) override {
      // draw
    }
  };

  struct GeometryPass : RenderPass {
    GraphicsShaderComponent shader_component;
    ResourceComponent resource_component;

    std::vector<std::shared_ptr<Resource>> get_resources(ResourceUsage usage) override {
      return resource_component.get_resources(usage);
    }

    GeometryPass(const std::shared_ptr<Resource>& g_buffer_1,
                 const std::shared_ptr<Resource>& g_buffer_2,
                 const std::shared_ptr<Resource>& g_buffer_3,
                 const std::shared_ptr<Resource>& g_buffer_depth,
                 const std::shared_ptr<Resource>& geometry_buffer
    ) {
      resource_component.addResource(geometry_buffer, ResourceUsage::READ);
      resource_component.addResource(g_buffer_1, ResourceUsage::WRITE);
      resource_component.addResource(g_buffer_2, ResourceUsage::WRITE);
      resource_component.addResource(g_buffer_3, ResourceUsage::WRITE);
      resource_component.addResource(g_buffer_depth, ResourceUsage::WRITE);
      name = "GeometryPass";
    }

    void compile() override {
      // Specific pipeline layout and pipeline creation for graphics
    }

    void execute(CommandBuffer cmd) override{
        // draw
    }
  };

  struct LightingPass : RenderPass {
    ComputeShaderComponent shader_component;
    ResourceComponent resource_component;

    LightingPass(const std::shared_ptr<Resource>& scene_lit,
                 const std::shared_ptr<Resource>& g_buffer_1,
                 const std::shared_ptr<Resource>& g_buffer_2,
                 const std::shared_ptr<Resource>& g_buffer_3,
                 const std::shared_ptr<Resource>& g_buffer_depth,
                 const std::shared_ptr<Resource>& shadow_map,
                 const std::shared_ptr<Resource>& material_buffer,
                 const std::shared_ptr<Resource>& light_buffer) {
      resource_component.addResource(scene_lit, ResourceUsage::WRITE);
      resource_component.addResource(g_buffer_1, ResourceUsage::READ);
      resource_component.addResource(g_buffer_2, ResourceUsage::READ);
      resource_component.addResource(g_buffer_3, ResourceUsage::READ);
      resource_component.addResource(g_buffer_depth, ResourceUsage::READ);
      resource_component.addResource(shadow_map, ResourceUsage::READ);
      resource_component.addResource(material_buffer, ResourceUsage::READ);
      resource_component.addResource(light_buffer, ResourceUsage::READ);
      name = "LightingPass";
    }

    std::vector<std::shared_ptr<Resource>> get_resources(ResourceUsage usage) override {
      return resource_component.get_resources(usage);
    }

    void compile() override {
      // Specific pipeline layout and pipeline creation for compute
    }

    void execute(CommandBuffer cmd) override {
      // draw
    }
  };

  struct ToneMapPass : RenderPass {
    ComputeShaderComponent shader_component;
    ResourceComponent resource_component;

    ToneMapPass(const std::shared_ptr<Resource>& scene_final, const std::shared_ptr<Resource>& scene_lit) {
      resource_component.addResource(scene_lit, ResourceUsage::READ);
      resource_component.addResource(scene_final, ResourceUsage::WRITE);
      name = "ToneMapPass";
    }

    std::vector<std::shared_ptr<Resource>> get_resources(ResourceUsage usage) override {
      return resource_component.get_resources(usage);
    }

    void compile() override {
      // Specific pipeline layout and pipeline creation for compute
    }

    void execute(CommandBuffer cmd) override {
      // draw
    }
  };

  struct ResourceInitializerPass : RenderPass {
    ComputeShaderComponent shader_component;
    ResourceComponent resource_component;

    ResourceInitializerPass(
        const std::shared_ptr<Resource>& geometry_buffer, 
        const std::shared_ptr<Resource>& material_buffer, 
        const std::shared_ptr<Resource>& light_buffer
        ) {
      resource_component.addResource(geometry_buffer, ResourceUsage::WRITE);
      resource_component.addResource(material_buffer, ResourceUsage::WRITE);
      resource_component.addResource(light_buffer, ResourceUsage::WRITE);
      name = "ResourceInitializerPass";
    }

    std::vector<std::shared_ptr<Resource>> get_resources(ResourceUsage usage) override {
      return resource_component.get_resources(usage);
    }

    void compile() override {
      // does nothing
    }

    void execute(CommandBuffer cmd) override {
      // does nothing
    }
  };

  struct FrameGraphNode;

  struct FrameGraphEdge {
    std::shared_ptr<Resource> resource;
    std::vector<std::shared_ptr<FrameGraphNode>> nodes_from;
    std::vector<std::shared_ptr<FrameGraphNode>> nodes_to;

    explicit FrameGraphEdge(Resource&& resource) : resource(std::make_shared<Resource>(std::move(resource))) {}
  };

  struct FrameGraphNode {
    std::shared_ptr<RenderPass> render_pass;
    std::vector<std::shared_ptr<FrameGraphEdge>> edges_in;
    std::vector<std::shared_ptr<FrameGraphEdge>> edges_out;

    explicit FrameGraphNode(std::shared_ptr<RenderPass>&& render_pass) : render_pass(std::move(render_pass)) {}

    std::vector<std::shared_ptr<FrameGraphNode>> get_neighbors() const {
      std::vector<std::shared_ptr<FrameGraphNode>> neighbors;
      for (const auto& edge : edges_out) {
        for (const auto& neighbor : edge->nodes_to) {
          neighbors.push_back(neighbor);
        }
      }
      return neighbors;
    }

  };

  struct DescriptorSetLayout {
    uint32 resource_handle;
  };

  struct DescriptorSet {
    std::vector<DescriptorSetLayout> layouts;
  };

  struct DescriptorBindingPoint {
    std::array<DescriptorSet, 10> descriptor_sets;
  };

  class DescriptorManger {
    DescriptorBindingPoint graphics_binding_point;
    DescriptorBindingPoint compute_binding_point;
    // manage descriptor sets and bindings on CPU side
  };

  class SynchronizationManager {
    // manage synchronization between resources
  };

  class ResourceRegistry {
    std::unordered_map<uint32, Resource> resource_map_;

    uint32 registerImage(Resource resource) {
      resource.handle = resource_map_.size();
      resource_map_.insert({resource.handle, resource});
      return resource.handle;
    }

    Resource get_resource(uint32 handle) { return resource_map_.at(handle); }
  };

  class FrameGraph : public NonCopyable<FrameGraph> {
    std::vector<std::shared_ptr<FrameGraphNode>> nodes_;
    std::unordered_map<uint32, std::shared_ptr<FrameGraphEdge>> edges_;

    std::vector<std::shared_ptr<FrameGraphNode>> sorted_nodes_;

    std::unique_ptr<DescriptorManger> descriptor_manger_;
    std::unique_ptr<SynchronizationManager> synchronization_manager_;
    std::unique_ptr<ResourceRegistry> resource_registry_;

    void print_graph() const {
      for (const auto& node : nodes_) {
        fmt::println("Node: {}", node->render_pass->name);
        fmt::println("needs:");
        for (const auto& edge : node->edges_in) {
          fmt::println("\t{}", edge->resource->name);
        }
        fmt::println("updates:");
        for (const auto& edge : node->edges_out) {
          fmt::println("\t{}", edge->resource->name);
        }
      }
    }

    void topological_sort() {
      std::unordered_map<std::shared_ptr<FrameGraphNode>, uint32_t> in_degree;
      for (auto& node : nodes_) {
        in_degree[node] = static_cast<uint32>(node->edges_in.size());
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

        for (const auto& neighbor : current_node->get_neighbors()) {
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

  public:
    FrameGraph() {
      nodes_.reserve(25);
      descriptor_manger_ = std::make_unique<DescriptorManger>();
      synchronization_manager_ = std::make_unique<SynchronizationManager>();
      resource_registry_ = std::make_unique<ResourceRegistry>();
    }

    void add_render_pass(std::shared_ptr<RenderPass>&& pass) {
      nodes_.push_back(std::make_shared<FrameGraphNode>(std::move(pass)));
    }

    // todo add to regsitry, create resource and return handle
    std::shared_ptr<Resource> add_resource(ImageResource&& resource) {
      auto& handle = resource.handle;
      resource.handle = edges_.size();
      edges_.insert({handle, std::make_shared<FrameGraphEdge>(std::move(resource))});
      return edges_.at(handle)->resource;
    }

    std::shared_ptr<Resource> add_resource(BufferResource&& resource) {
      auto& handle = resource.handle;
      resource.handle = edges_.size();
      edges_.insert({handle, std::make_shared<FrameGraphEdge>(std::move(resource))});
      return edges_.at(handle)->resource;
    }

    void compile() {
      fmt::print("Compiling FrameGraph\n");
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

    void execute() {
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
  };

}  // namespace gestalt::graphics::fg