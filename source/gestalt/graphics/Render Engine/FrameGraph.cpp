#pragma once
#include <array>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include "common.hpp"

namespace gestalt::graphics::fg {

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
    std::vector<Resource*> resources;
    PushDescriptor push_descriptor;
  };

  class CommandBuffer {
    // vkCommandBuffer command_buffer;
    // draw()
    // dispatch()
  };

  struct ShaderComponent {
    virtual void execute(CommandBuffer cmd) = 0;
    virtual ~ShaderComponent() = default;
  };

  struct GraphicsShaderComponent : ShaderComponent {
    void execute(CommandBuffer cmd) override;
    // bind resources to descriptor sets
    // bind pipeline to command buffer
    // draw
  };

  struct ComputeShaderComponent : ShaderComponent {
    void execute(CommandBuffer cmd) override;
    // bind resources to descriptor sets
    // bind pipeline to command buffer
    // dispatch
  };

  enum class ResourceUsage { READ, WRITE };

  struct ResourceComponent {
    void addResource(Resource& resource, ResourceUsage usage) {
      if (usage == ResourceUsage::READ) {
        read_resources.resources.push_back(&resource);
      } else if (usage == ResourceUsage::WRITE) {
        write_resources.resources.push_back(&resource);
      }
    }

    std::vector<Resource*> get_resources(ResourceUsage usage) {
      if (usage == ResourceUsage::READ) {
        return read_resources.resources;
      } else if (usage == ResourceUsage::WRITE) {
        return write_resources.resources;
      }
    }
  private:
    Resources read_resources;
    Resources write_resources;
  };

  struct RenderPass {
    virtual void compile() = 0;  // Pure virtual method for pipeline creation
    virtual std::vector<Resource*> get_resources(ResourceUsage usage) = 0;
    virtual void execute(CommandBuffer cmd) = 0;  // Pure virtual method for execution
    virtual ~RenderPass() = default;
    std::string name;
  };

  struct ShadowMapPass : RenderPass {
    ComputeShaderComponent shader_component;
    ResourceComponent resource_component;

    ShadowMapPass(Resource& shadow_map, Resource& geometry_buffer) {
      resource_component.addResource(shadow_map, ResourceUsage::WRITE);
      resource_component.addResource(geometry_buffer, ResourceUsage::READ);
      name = "ShadowMapPass";
    }

    std::vector<Resource*> get_resources(ResourceUsage usage) override {
      return resource_component.get_resources(usage);
    }

    void compile() override {
      // Specific pipeline layout and pipeline creation for compute
    }

    void execute(CommandBuffer cmd) override { shader_component.execute(cmd); }
  };

  struct GeometryPass : RenderPass {
    GraphicsShaderComponent shader_component;
    ResourceComponent resource_component;

    std::vector<Resource*> get_resources(ResourceUsage usage) override {
      return resource_component.get_resources(usage);
    }

    GeometryPass(Resource& g_buffer_1, Resource& g_buffer_2, Resource& g_buffer_3,
                 Resource& g_buffer_depth) {
      resource_component.addResource(g_buffer_1, ResourceUsage::WRITE);
      resource_component.addResource(g_buffer_2, ResourceUsage::WRITE);
      resource_component.addResource(g_buffer_3, ResourceUsage::WRITE);
      resource_component.addResource(g_buffer_depth, ResourceUsage::WRITE);
      name = "GeometryPass";
    }

    void compile() override {
      // Specific pipeline layout and pipeline creation for graphics
    }

    void execute(CommandBuffer cmd) override { shader_component.execute(cmd); }
  };

  struct LightingPass : RenderPass {
    ComputeShaderComponent shader_component;
    ResourceComponent resource_component;

    LightingPass(Resource& scene_lit, Resource& g_buffer_1, Resource& g_buffer_2,
                 Resource& g_buffer_3, Resource& g_buffer_depth, Resource& material_buffer,
                 Resource& light_buffer) {
      resource_component.addResource(scene_lit, ResourceUsage::WRITE);
      resource_component.addResource(g_buffer_1, ResourceUsage::READ);
      resource_component.addResource(g_buffer_2, ResourceUsage::READ);
      resource_component.addResource(g_buffer_3, ResourceUsage::READ);
      resource_component.addResource(g_buffer_depth, ResourceUsage::READ);
      resource_component.addResource(material_buffer, ResourceUsage::READ);
      resource_component.addResource(light_buffer, ResourceUsage::READ);
      name = "LightingPass";
    }

    std::vector<Resource*> get_resources(ResourceUsage usage) override {
      return resource_component.get_resources(usage);
    }

    void compile() override {
      // Specific pipeline layout and pipeline creation for compute
    }

    void execute(CommandBuffer cmd) override { shader_component.execute(cmd); }
  };

  struct ToneMapPass : RenderPass {
    ComputeShaderComponent shader_component;
    ResourceComponent resource_component;

    ToneMapPass(Resource& scene_final, Resource& scene_lit) {
      resource_component.addResource(scene_final, ResourceUsage::READ);
      resource_component.addResource(scene_lit, ResourceUsage::WRITE);
      name = "ToneMapPass";
    }

    std::vector<Resource*> get_resources(ResourceUsage usage) override {
      return resource_component.get_resources(usage);
    }

    void compile() override {
      // Specific pipeline layout and pipeline creation for compute
    }

    void execute(CommandBuffer cmd) override { shader_component.execute(cmd); }
  };

  struct PassNode;

  struct ResourceEdge {
    Resource* resource;
    std::vector<PassNode*> edges_in;
    std::vector<PassNode*> edges_out;

    ResourceEdge(Resource* resource) : resource(resource) {}
  };

  struct PassNode {
    RenderPass* pass;
    std::vector<ResourceEdge*> edges_in;
    std::vector<ResourceEdge*> edges_out;

    explicit PassNode(RenderPass& pass) : pass(&pass) {}

    bool operator==(const PassNode& other) const { return pass == other.pass; }

    std::set<PassNode> getAdj() {
      std::set<PassNode> adj;
      for (auto e : edges_out) {
        adj.insert(e->edges_out.begin(), e->edges_out.end());
      }
      return adj;
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
    std::vector<PassNode> nodes_;
    std::unordered_map<uint32, ResourceEdge> edges_;
    std::unordered_map<PassNode, std::set<PassNode>> adj_;

    std::unique_ptr<DescriptorManger> descriptor_manger_;
    std::unique_ptr<SynchronizationManager> synchronization_manager_;
    std::unique_ptr<ResourceRegistry> resource_registry_;

  public:
    FrameGraph() {
      nodes_.reserve(25);
      descriptor_manger_ = std::make_unique<DescriptorManger>();
      synchronization_manager_ = std::make_unique<SynchronizationManager>();
      resource_registry_ = std::make_unique<ResourceRegistry>();
    }

    void addNode(RenderPass& pass) { nodes_.emplace_back(pass); }

    void addEdge(Resource& resource) { edges_.insert({resource.handle, {&resource}}); }
    void addEdge(ImageResource& resource) { edges_.insert({resource.handle, {&resource}}); }
    void addEdge(BufferResource& resource) { edges_.insert({resource.handle, {&resource}}); }

    void compile() {
      for (auto& node : nodes_) {
        for (const auto read_resource : node.pass->get_resources(ResourceUsage::READ)) {
          auto& edge = edges_.at(read_resource->handle);
          edge.edges_out.push_back(&node);
          node.edges_in.push_back(&edge);
        }
        for (const auto write_resource : node.pass->get_resources(ResourceUsage::WRITE)) {
          auto& edge = edges_.at(write_resource->handle);
          edge.edges_in.push_back(&node);
          node.edges_out.push_back(&edge);
        }
      }

      for (auto& node : nodes_) {
        adj_.insert({node, node.getAdj()});
      }
    }

    std::vector<PassNode> topological_sort() {
      std::unordered_map<PassNode, uint32> in_degree;
      for (auto& node : nodes_) {
        in_degree.insert({node, node.edges_in.size()});
      }

      std::queue<PassNode> zero_in_degree;
      for (auto& node : nodes_) {
        if (in_degree.at(node) == 0) {
          zero_in_degree.push(node);
        }
      }

      std::vector<PassNode> sorted;

      while (!zero_in_degree.empty()) {
        auto current = zero_in_degree.front();
        zero_in_degree.pop();
        sorted.push_back(current);

        for (auto& edge : current.edges_out) {
          for (auto& neighbor : edge->edges_out) {
            in_degree.at(*neighbor) -= 1;
            if (in_degree.at(*neighbor) == 0) {
              zero_in_degree.push(*neighbor);
            }
          }
        }
      }

      if (sorted.size() != nodes_.size()) {
        assert(!"Cycle detected in the graph! Topological sorting is not possible.");
      }

      return sorted;
    }

    void execute() {
      // run each render pass in order and synchronize resources
      // for (auto& pass_node : pass_nodes_) {
      // bind resources to descriptor sets
      // sync resources
      // pass_node.pass.execute(pass_node.inputs...)
    }
  };

}  // namespace gestalt::graphics::fg

#ifndef FRAMEGRAPH_HASH_DEFINED
#  define FRAMEGRAPH_HASH_DEFINED

namespace std {
  template <> struct hash<gestalt::graphics::fg::PassNode> {
    size_t operator()(const gestalt::graphics::fg::PassNode& node) const noexcept {
      return std::hash<gestalt::graphics::fg::RenderPass*>()(
          node.pass);  // Hash the RenderPass pointer
    }
  };
}  // namespace std

#endif  // FRAMEGRAPH_HASH_DEFINED