#pragma once
#include <array>
#include <memory>
#include <unordered_map>
#include <vector>

#include "common.hpp"
#include "fmt/core.h"

namespace gestalt::graphics::fg {

  struct ResourceTemplate {
    std::string name;

    explicit ResourceTemplate(std::string name) : name(std::move(name)) {}

    virtual ~ResourceTemplate() = default;
  };

  enum class ImageType { kImage2D, kImage3D, kCubeMap };

  // scaled from window/screen resolution
  struct RelativeImageSize {
    float32 scale{1.0f};

    explicit RelativeImageSize(const float32 scale = 1.0f) : scale(scale) {
      assert(scale > 0.0f && "Scale must be positive.");
      assert(scale <= 16.0f && "Scale cannot be higher than 16.0.");
    }
  };

  // fixed dimensions
  struct AbsoluteImageSize {
    VkExtent3D extent{0, 0, 0};

    AbsoluteImageSize(uint32 width, uint32 height, uint32 depth = 0) {
      extent = {width, height, depth};
      assert(width > 0 && height > 0 && "Width and height must be positive.");
    }
  };

  struct ImageResourceTemplate final : ResourceTemplate {
    ImageType image_type = ImageType::kImage2D;
    TextureType type = TextureType::kColor;
    std::variant<VkClearValue, std::filesystem::path> initial_value
        = VkClearValue({.color = {0.f, 0.f, 0.f, 1.f}});
    std::variant<RelativeImageSize, AbsoluteImageSize> image_size = RelativeImageSize(1.f);
    VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;

    // Constructor with name and optional customization
    explicit ImageResourceTemplate(std::string name) : ResourceTemplate(std::move(name)) {}

    // Fluent setters for customization
    ImageResourceTemplate& set_image_type(const TextureType texture_type, const VkFormat format) {
      this->type = texture_type;
      this->format = format;
      return *this;
    }

    ImageResourceTemplate& set_initial_value(const VkClearColorValue& clear_value) {
      assert(type == TextureType::kColor && "Clear color only supported for color images.");
      this->initial_value = VkClearValue({.color = clear_value});
      return *this;
    }

    ImageResourceTemplate& set_initial_value(const VkClearDepthStencilValue& clear_value) {
      assert(type == TextureType::kDepth && "Clear depth only supported for depth images.");
      this->initial_value = VkClearValue({.depthStencil = clear_value});
      return *this;
    }

    ImageResourceTemplate& set_initial_value(const std::filesystem::path& path) {
      assert(type == TextureType::kColor && "path only supported for color images.");
      this->initial_value = path;
      return *this;
    }

    ImageResourceTemplate& set_image_size(const float32& relative_size) {
      this->image_size = RelativeImageSize(relative_size);
      return *this;
    }

    ImageResourceTemplate& set_image_size(const uint32 width, const uint32 height, const uint32 depth = 0) {
      this->image_size = AbsoluteImageSize(width, height, depth);
      return *this;
    }

    ImageResourceTemplate build() { return *this; }

  };


  struct BufferResourceTemplate : ResourceTemplate {
    VkDeviceSize size;
    VkBufferUsageFlags usage;
    VmaMemoryUsage memory_usage;

    BufferResourceTemplate(std::string name)
        : ResourceTemplate(std::move(name)) {}
  };

  //TODO split into template and instance
  struct Resource {
    uint32 handle = -1;
    ResourceTemplate resource_template;
    explicit Resource(ResourceTemplate&& resource_template) : resource_template(resource_template) {}
    std::string_view name() const { return resource_template.name; }
  };

  struct ImageResource : Resource {
    using Resource::Resource; // Inherit constructor

    std::shared_ptr<TextureHandle> image;
  };

  struct BufferResource : Resource {
    using Resource::Resource; // Inherit constructor
    std::shared_ptr<AllocatedBuffer> buffer;
  };

  struct PushDescriptor {
    // vkPushDescriptor push_descriptor;
  };

  struct Resources {
    std::vector<std::shared_ptr<Resource>> resources;
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
    void add_resource(const std::shared_ptr<Resource>& resource, const ResourceUsage usage) {
      if (usage == ResourceUsage::READ) {
        read_resources.resources.push_back(resource);
      } else if (usage == ResourceUsage::WRITE) {
        write_resources.resources.push_back(resource);
      }
    }

    std::vector<std::shared_ptr<Resource>> get_resources(const ResourceUsage usage) {
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
      resource_component.add_resource(geometry_buffer, ResourceUsage::READ);
      resource_component.add_resource(shadow_map, ResourceUsage::WRITE);
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

    LightingPass(const std::shared_ptr<Resource>& scene_lit,
                 const std::shared_ptr<Resource>& g_buffer_1,
                 const std::shared_ptr<Resource>& g_buffer_2,
                 const std::shared_ptr<Resource>& g_buffer_3,
                 const std::shared_ptr<Resource>& g_buffer_depth,
                 const std::shared_ptr<Resource>& shadow_map,
                 const std::shared_ptr<Resource>& material_buffer,
                 const std::shared_ptr<Resource>& light_buffer) {
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

  struct SsaoPass : RenderPass {
    ComputeShaderComponent shader_component;
    ResourceComponent resource_component;

    SsaoPass(const std::shared_ptr<Resource>& g_buffer_depth,
             const std::shared_ptr<Resource>& g_buffer_2,
             const std::shared_ptr<Resource>& rotation_texture,
             const std::shared_ptr<Resource>& occlusion) {
      resource_component.add_resource(g_buffer_depth, ResourceUsage::READ);
      resource_component.add_resource(g_buffer_2, ResourceUsage::READ);
      resource_component.add_resource(rotation_texture, ResourceUsage::READ);
      resource_component.add_resource(occlusion, ResourceUsage::WRITE);
      name = "SsaoPass";
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
      resource_component.add_resource(scene_lit, ResourceUsage::READ);
      resource_component.add_resource(scene_final, ResourceUsage::WRITE);
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

  struct FrameGraphNode;

  // defines if the resource is used internal or external
  // internal resources are created and managed by the frame graph
  // external resources are created and managed by some other system
  // external resources may not count as dependencies for the frame graph
  enum class CreationType { INTERNAL, EXTERNAL };

  struct FrameGraphEdge {
    std::shared_ptr<Resource> resource;
    std::vector<std::shared_ptr<FrameGraphNode>> nodes_from;
    std::vector<std::shared_ptr<FrameGraphNode>> nodes_to;
    CreationType creation_type;

    explicit FrameGraphEdge(const std::shared_ptr<Resource>& resource, const CreationType creation_type)
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
    std::unordered_map<uint32, std::shared_ptr<Resource>> resource_map_;

  public:
    std::shared_ptr<Resource> add(ImageResourceTemplate&& resource) {
      std::shared_ptr<Resource> image = std::make_shared<ImageResource>(std::move(resource));
      image->handle = static_cast<uint32>(resource_map_.size());
      resource_map_.insert({image->handle, image});
      return image;
    }

    std::shared_ptr<Resource> add(BufferResourceTemplate&& resource) {
      std::shared_ptr<Resource> image = std::make_shared<BufferResource>(std::move(resource));
      image->handle = static_cast<uint32>(resource_map_.size());
      resource_map_.insert({image->handle, image});
      return image;
    }

    std::shared_ptr<Resource> get_resource(const uint32 handle) { return resource_map_.at(handle); }
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


    void topological_sort() {
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

    template <typename ResourceTemplateType>
    auto add_resource(ResourceTemplateType&& resource_template,
                      CreationType creation_type = CreationType::INTERNAL) {
      auto resource
          = resource_registry_->add(std::forward<ResourceTemplateType>(resource_template));
      edges_.insert({resource->handle, std::make_shared<FrameGraphEdge>(std::move(resource),
                                                                        creation_type)});
      return resource;
    }

    void compile() {
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