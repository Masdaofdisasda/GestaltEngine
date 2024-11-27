#pragma once
#include <array>
#include <memory>
#include <queue>
#include <unordered_map>
#include <vector>

#include "common.hpp"
#include "Resources/ResourceTypes.hpp"

namespace gestalt::graphics {
  class ResourceAllocator;
}

namespace gestalt::graphics::fg {

  struct PushDescriptor {
    // vkPushDescriptor push_descriptor;
  };

  struct Resources {
    std::vector<std::shared_ptr<ResourceInstance>> resources;
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
    void add_resource(const std::shared_ptr<ResourceInstance>& resource, const ResourceUsage usage) {
      if (usage == ResourceUsage::READ) {
        read_resources.resources.push_back(resource);
      } else if (usage == ResourceUsage::WRITE) {
        write_resources.resources.push_back(resource);
      }
    }

    std::vector<std::shared_ptr<ResourceInstance>> get_resources(const ResourceUsage usage) {
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
    PushDescriptor push_descriptor;
  };

  struct RenderPass {
    virtual void compile() = 0;
    virtual std::vector<std::shared_ptr<ResourceInstance>> get_resources(ResourceUsage usage) = 0;
    virtual void execute(CommandBuffer cmd) = 0;
    virtual ~RenderPass() = default;
    std::string name;
  };

  struct ShadowMapPass : RenderPass {
    ComputeShaderComponent shader_component;
    ResourceComponent resource_component;

    ShadowMapPass(const std::shared_ptr<ResourceInstance>& shadow_map,
                  const std::shared_ptr<ResourceInstance>& geometry_buffer) {
      resource_component.add_resource(geometry_buffer, ResourceUsage::READ);
      resource_component.add_resource(shadow_map, ResourceUsage::WRITE);
      name = "ShadowMapPass";
    }

    std::vector<std::shared_ptr<ResourceInstance>> get_resources(ResourceUsage usage) override {
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

    std::vector<std::shared_ptr<ResourceInstance>> get_resources(ResourceUsage usage) override {
      return resource_component.get_resources(usage);
    }

    GeometryPass(const std::shared_ptr<ResourceInstance>& g_buffer_1,
                 const std::shared_ptr<ResourceInstance>& g_buffer_2,
                 const std::shared_ptr<ResourceInstance>& g_buffer_3,
                 const std::shared_ptr<ResourceInstance>& g_buffer_depth,
                 const std::shared_ptr<ResourceInstance>& geometry_buffer
    ) {
      resource_component.add_resource(geometry_buffer, ResourceUsage::READ);
      resource_component.add_resource(g_buffer_1, ResourceUsage::WRITE);
      resource_component.add_resource(g_buffer_2, ResourceUsage::WRITE);
      resource_component.add_resource(g_buffer_3, ResourceUsage::WRITE);
      resource_component.add_resource(g_buffer_depth, ResourceUsage::WRITE);
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

    LightingPass(const std::shared_ptr<ResourceInstance>& scene_lit,
                 const std::shared_ptr<ResourceInstance>& g_buffer_1,
                 const std::shared_ptr<ResourceInstance>& g_buffer_2,
                 const std::shared_ptr<ResourceInstance>& g_buffer_3,
                 const std::shared_ptr<ResourceInstance>& g_buffer_depth,
                 const std::shared_ptr<ResourceInstance>& shadow_map,
                 const std::shared_ptr<ResourceInstance>& material_buffer,
                 const std::shared_ptr<ResourceInstance>& light_buffer) {
      resource_component.add_resource(scene_lit, ResourceUsage::WRITE);
      resource_component.add_resource(g_buffer_1, ResourceUsage::READ);
      resource_component.add_resource(g_buffer_2, ResourceUsage::READ);
      resource_component.add_resource(g_buffer_3, ResourceUsage::READ);
      resource_component.add_resource(g_buffer_depth, ResourceUsage::READ);
      resource_component.add_resource(shadow_map, ResourceUsage::READ);
      resource_component.add_resource(material_buffer, ResourceUsage::READ);
      resource_component.add_resource(light_buffer, ResourceUsage::READ);
      name = "LightingPass";
    }

    std::vector<std::shared_ptr<ResourceInstance>> get_resources(ResourceUsage usage) override {
      return resource_component.get_resources(usage);
    }

    void compile() override {
      // Specific pipeline layout and pipeline creation for compute
    }

    void execute(CommandBuffer cmd) override {
      // draw
    }
  };

  struct SsaoPass : RenderPass {
    ComputeShaderComponent shader_component;
    ResourceComponent resource_component;

    SsaoPass(const std::shared_ptr<ResourceInstance>& g_buffer_depth,
             const std::shared_ptr<ResourceInstance>& g_buffer_2,
             const std::shared_ptr<ResourceInstance>& rotation_texture,
             const std::shared_ptr<ResourceInstance>& occlusion) {
      resource_component.add_resource(g_buffer_depth, ResourceUsage::READ);
      resource_component.add_resource(g_buffer_2, ResourceUsage::READ);
      resource_component.add_resource(rotation_texture, ResourceUsage::READ);
      resource_component.add_resource(occlusion, ResourceUsage::WRITE);
      name = "SsaoPass";
    }

    std::vector<std::shared_ptr<ResourceInstance>> get_resources(ResourceUsage usage) override {
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

    ToneMapPass(const std::shared_ptr<ResourceInstance>& scene_final, const std::shared_ptr<ResourceInstance>& scene_lit) {
      resource_component.add_resource(scene_lit, ResourceUsage::READ);
      resource_component.add_resource(scene_final, ResourceUsage::WRITE);
      name = "ToneMapPass";
    }

    std::vector<std::shared_ptr<ResourceInstance>> get_resources(ResourceUsage usage) override {
      return resource_component.get_resources(usage);
    }

    void compile() override {
      // Specific pipeline layout and pipeline creation for compute
    }

    void execute(CommandBuffer cmd) override {
      // draw
    }
  };

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

    explicit FrameGraphEdge(const std::shared_ptr<ResourceInstance>& resource, const CreationType creation_type)
        : resource(resource), creation_type(creation_type) {}
  };

  struct FrameGraphNode {
    std::shared_ptr<RenderPass> render_pass;
    std::vector<std::shared_ptr<FrameGraphEdge>> edges_in;
    std::vector<std::shared_ptr<FrameGraphEdge>> edges_out;

    explicit FrameGraphNode(std::shared_ptr<RenderPass>&& render_pass) : render_pass(std::move(render_pass)) {}

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
    std::unordered_map<uint64, std::shared_ptr<ResourceInstance>> resource_map_;
    ResourceAllocator* resource_factory_ = nullptr;

  public:
    explicit ResourceRegistry(ResourceAllocator* resource_factory) : resource_factory_(resource_factory) {}

    std::shared_ptr<ResourceInstance> add_template(ImageTemplate&& image_template);

    std::shared_ptr<ResourceInstance> add_template(BufferTemplate&& buffer_template);

    std::shared_ptr<ResourceInstance> add_resource(std::shared_ptr<ImageInstance> image_resource);

    std::shared_ptr<ResourceInstance> add_resource(std::shared_ptr<BufferInstance> buffer_resource);

    std::shared_ptr<ResourceInstance> get_resource(const uint32 handle) { return resource_map_.at(handle); }
  };

  class FrameGraph : public NonCopyable<FrameGraph> {
    std::vector<std::shared_ptr<FrameGraphNode>> nodes_;
    std::unordered_map<uint32, std::shared_ptr<FrameGraphEdge>> edges_;

    std::vector<std::shared_ptr<FrameGraphNode>> sorted_nodes_;

    std::unique_ptr<DescriptorManger> descriptor_manger_;
    std::unique_ptr<SynchronizationManager> synchronization_manager_;
    std::unique_ptr<ResourceRegistry> resource_registry_;

    void print_graph() const;


    void topological_sort();

  public:
    explicit FrameGraph(ResourceAllocator* resource_factory);

    void add_render_pass(std::shared_ptr<RenderPass>&& pass);

    template <typename ResourceTemplateType>
    auto add_resource(ResourceTemplateType&& resource_template,
                      CreationType creation_type = CreationType::INTERNAL) {
      auto resource
          = resource_registry_->add_template(std::forward<ResourceTemplateType>(resource_template));
      edges_.insert({resource->resource_handle, std::make_shared<FrameGraphEdge>(std::move(resource),
                                                                        creation_type)});
      return resource;
    }

    template <typename ResourceInstanceType>
    auto add_resource(std::shared_ptr<ResourceInstanceType> resource_instance,
                      CreationType creation_type = CreationType::EXTERNAL) {
      auto resource = resource_registry_->add_resource(resource_instance);
      edges_.insert({resource->resource_handle,
                     std::make_shared<FrameGraphEdge>(std::move(resource), creation_type)});
      return resource;
    }

    void compile();

    void execute();
  };

}  // namespace gestalt::graphics::fg