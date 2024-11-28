#pragma once
#include <array>
#include <memory>
#include <ranges>
#include <unordered_map>
#include <vector>

#include "ResourceTypes.hpp"
#include "VulkanCheck.hpp"
#include "common.hpp"
#include "vk_initializers.hpp"
#include "Interface/IGpu.hpp"
#include "Resources/ResourceTypes.hpp"
#include "Utils/vk_pipelines.hpp"

namespace gestalt::graphics {
  class ResourceAllocator;
}

namespace gestalt::graphics::fg {

  struct PushDescriptor {

    PushDescriptor(const uint32_t size, const VkShaderStageFlags stage_flags) {
      push_constant_range.size = size;
      push_constant_range.offset = 0;
      push_constant_range.stageFlags = stage_flags;
    }

    PushDescriptor() {
      push_constant_range.size = 0;
      push_constant_range.offset = 0;
      push_constant_range.stageFlags = 0;
    }

    VkPushConstantRange get_push_constant_range() const { return push_constant_range; }

  private:

    VkPushConstantRange push_constant_range = {.size = 0};
  };

  struct Resources {
    std::vector<std::shared_ptr<ResourceInstance>> resources;
  };

  class CommandBuffer {
    VkCommandBuffer cmd;
  public:
    explicit CommandBuffer(const VkCommandBuffer cmd) : cmd(cmd) {}

    void begin_rendering(const VkRenderingInfo& render_pass_info) const {
      vkCmdBeginRendering(cmd, &render_pass_info);
    }

    void end_rendering() const { vkCmdEndRendering(cmd); }

    void bind_pipeline(const VkPipelineBindPoint bind_point, const VkPipeline pipeline) const {
      vkCmdBindPipeline(cmd, bind_point, pipeline);
    }

    void pipeline_barrier(const VkPipelineStageFlags src_stage_mask,
                          const VkPipelineStageFlags dst_stage_mask,
                          const VkDependencyFlags dependency_flags,
                          const uint32 memory_barrier_count, const VkMemoryBarrier* memory_barriers,
                          const uint32 buffer_memory_barrier_count,
                          const VkBufferMemoryBarrier* buffer_memory_barriers,
                          const uint32 image_memory_barrier_count,
                          const VkImageMemoryBarrier* image_memory_barriers) const {
      vkCmdPipelineBarrier(cmd, src_stage_mask, dst_stage_mask, dependency_flags,
                           memory_barrier_count, memory_barriers, buffer_memory_barrier_count,
                           buffer_memory_barriers, image_memory_barrier_count,
                           image_memory_barriers);
    }

    void fill_buffer(const VkBuffer buffer, const VkDeviceSize offset, const VkDeviceSize size,
                     const uint32 data) const {
      vkCmdFillBuffer(cmd, buffer, offset, size, data);
    }

    void set_viewport(const uint32 first_viewport, const uint32 viewport_count,
                      const VkViewport* viewports) const {
      vkCmdSetViewport(cmd, first_viewport, viewport_count, viewports);
    }

    void set_scissor(const uint32 first_scissor, const uint32 scissor_count,
                     const VkRect2D* scissors) const {
      vkCmdSetScissor(cmd, first_scissor, scissor_count, scissors);
    }

    void push_constants(const VkPipelineLayout layout, const VkShaderStageFlags stage_flags,
                        const uint32 offset, const uint32 size, const void* data) const {
      vkCmdPushConstants(cmd, layout, stage_flags, offset, size, data);
    }

    void draw_mesh_tasks_indirect_ext(const VkBuffer buffer, const VkDeviceSize offset,
                                      const uint32 draw_count, const uint32 stride) const {
      vkCmdDrawMeshTasksIndirectEXT(cmd, buffer, offset, draw_count, stride);
    }

    void dispatch(const uint32 group_count_x, const uint32 group_count_y,
                  const uint32 group_count_z) const {
      vkCmdDispatch(cmd, group_count_x, group_count_y, group_count_z);
    }
  };

  struct DescriptorSetLayoutBuilder {
    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;

    DescriptorSetLayoutBuilder& binding(uint32_t binding_index, VkDescriptorType descriptor_type,
                                        VkShaderStageFlags shader_stages,
                                        uint32_t descriptor_count = 1) {
      bindings.emplace(binding_index, VkDescriptorSetLayoutBinding{
                                          .binding = binding_index,
                                          .descriptorType = descriptor_type,
                                          .descriptorCount = descriptor_count,
                                          .stageFlags = shader_stages,
                                      });
      return *this;
    }

    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> get() { return std::move(bindings); }
  };

  struct DescriptorLayoutBuilder {
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>>
        set_bindings;

    DescriptorLayoutBuilder& set(
        uint32_t set_index, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>&& bindings) {
      set_bindings.emplace(set_index, std::move(bindings));
      return *this;
    }

    std::unordered_map<uint32_t, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>> get() {
      return std::move(set_bindings);
    }
  };


  class ShaderComponent {

    std::unordered_map<uint32, std::unordered_map<uint32, VkDescriptorSetLayoutBinding>>
        set_bindings_;
    VkPipelineLayout pipeline_layout_ = nullptr;
    VkPipeline pipeline_ = nullptr;

  public:
    virtual ~ShaderComponent() = default;

    VkPipelineLayout get_pipeline_layout() const { return pipeline_layout_; }

    void set_pipeline(const VkPipeline pipeline) { pipeline_ = pipeline; }

  protected:
    std::unordered_map<uint32, VkDescriptorSetLayout> descriptor_set_layouts;

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;

    VkShaderModule get_shader(const std::string&& source_file, const IGpu* gpu) {
      const std::string& shader_path = "../shaders/" + source_file;

      VkShaderModule shader_module;
      vkutil::load_shader_module(shader_path.c_str(), gpu->getDevice(), &shader_module);

      return shader_module;
    }

    void create_descriptor_layout(
        const IGpu* gpu,
        std::unordered_map<uint32, std::unordered_map<uint32, VkDescriptorSetLayoutBinding>>&&
            sets) {
      this->set_bindings_ = std::move(sets);

      for (const auto& [set_index, bindings] : set_bindings_) {

        std::vector<VkDescriptorSetLayoutBinding> binding_vector;
        binding_vector.reserve(bindings.size());
        for (const auto& binding : bindings | std::views::values) {
          binding_vector.push_back(binding);
        }

        VkDescriptorSetLayoutCreateInfo info
            = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
               .pNext = nullptr,
               .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
               .bindingCount = static_cast<uint32>(binding_vector.size()),
               .pBindings = binding_vector.data()};

        VkDescriptorSetLayout set;
        VK_CHECK(vkCreateDescriptorSetLayout(gpu->getDevice(), &info, nullptr, &set));
        descriptor_set_layouts.emplace(set_index, set);

      }

    }

      void create_pipeline_layout(const IGpu* gpu, const std::string_view pipeline_name, const VkPushConstantRange push_constant_range) {
      std::vector<VkDescriptorSetLayout> descriptor_set_layouts_vector;
      for (const auto& layout : descriptor_set_layouts | std::views::values) {
        descriptor_set_layouts_vector.push_back(layout);
      }

      VkPipelineLayoutCreateInfo pipeline_layout_create_info{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
          .pNext = nullptr,
          .setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts_vector.size()),
          .pSetLayouts = descriptor_set_layouts_vector.data(),
      };

      if (push_constant_range.size != 0) {
        pipeline_layout_create_info.pushConstantRangeCount = 1;
        pipeline_layout_create_info.pPushConstantRanges = &push_constant_range;
      }

      VkPipelineLayout pipeline_layout;
      VK_CHECK(vkCreatePipelineLayout(gpu->getDevice(), &pipeline_layout_create_info, nullptr,
                                      &pipeline_layout));
      pipeline_layout_ = pipeline_layout;

      gpu->set_debug_name(pipeline_name, VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                          reinterpret_cast<uint64_t>(pipeline_layout_));
    }

  };

  struct GraphicsShaderComponent : ShaderComponent {
    VkPipelineInputAssemblyStateCreateInfo _inputAssembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    VkPipelineRasterizationStateCreateInfo _rasterizer{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    std::vector<VkPipelineColorBlendAttachmentState> _colorBlendAttachments;
    VkPipelineMultisampleStateCreateInfo _multisampling{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    VkPipelineLayout _pipelineLayout{};
    VkPipelineDepthStencilStateCreateInfo _depthStencil{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    VkPipelineRenderingCreateInfo _renderInfo{.sType
                                              = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    std::vector<VkFormat> _colorAttachmentformats;
    std::vector<VkDynamicState> _dynamicStates;

    GraphicsShaderComponent& set_descriptor_set(
        const IGpu* gpu,
        std::unordered_map<uint32, std::unordered_map<uint32, VkDescriptorSetLayoutBinding>>&&
            sets) {
      create_descriptor_layout(gpu, std::move(sets));
      return *this;
    }

    GraphicsShaderComponent& set_vertex_shading(const std::string&& vertex_source,const std::string&& fragment_source, const IGpu* gpu) {
      shader_stages.clear();

      const auto vertex_shader = get_shader(std::move(vertex_source), gpu);
      const auto fragment_shader = get_shader(std::move(fragment_source), gpu);

      shader_stages.push_back(
          vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertex_shader));
      shader_stages.push_back(
          vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader));
      return *this;
    }

    GraphicsShaderComponent& set_mesh_task_shading(const std::string&& task_source,
                                                   const std::string&& mesh_source,
                                                   const std::string&& fragment_source,
                                                   const IGpu* gpu) {
      shader_stages.clear();

      const auto task_shader = get_shader(std::move(task_source), gpu);
      const auto mesh_shader = get_shader(std::move(mesh_source), gpu);
      const auto fragment_shader = get_shader(std::move(fragment_source), gpu);

      shader_stages.push_back(
          vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_TASK_BIT_EXT, task_shader));

      shader_stages.push_back(
          vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_MESH_BIT_EXT, mesh_shader));

      shader_stages.push_back(
          vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader));
       
      return *this;
    }

    GraphicsShaderComponent& set_input_topology(const VkPrimitiveTopology topology) {
      _inputAssembly.topology = topology;
      _inputAssembly.primitiveRestartEnable = VK_FALSE;

      return *this;
    }

    GraphicsShaderComponent& set_polygon_mode(const VkPolygonMode mode) {
      _rasterizer.polygonMode = mode;
      _rasterizer.lineWidth = 1.f;

      return *this;
    }

    GraphicsShaderComponent& set_cull_mode(const VkCullModeFlags cull_mode,
                                           const VkFrontFace front_face) {
      _rasterizer.cullMode = cull_mode;
      _rasterizer.frontFace = front_face;

      return *this;
    }

    GraphicsShaderComponent& set_multisampling_none() {
      _multisampling.sampleShadingEnable = VK_FALSE;
      // multisampling defaulted to no multisampling (1 sample per pixel)
      _multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
      _multisampling.minSampleShading = 1.0f;
      _multisampling.pSampleMask = nullptr;
      // no alpha to coverage either
      _multisampling.alphaToCoverageEnable = VK_FALSE;
      _multisampling.alphaToOneEnable = VK_FALSE;

      return *this;
    }

    GraphicsShaderComponent& disable_blending(uint32_t count) {
      constexpr VkPipelineColorBlendAttachmentState attachment = {
          .blendEnable = VK_FALSE,
          .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      };

      for (uint32_t i = 0; i < count; i++) {
        _colorBlendAttachments.push_back(attachment);
      }

      return *this;
    }

    GraphicsShaderComponent& enable_blending_additive() {
      constexpr VkPipelineColorBlendAttachmentState attachment = {
          .blendEnable = VK_TRUE,
          .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
          .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
          .colorBlendOp = VK_BLEND_OP_ADD,
          .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
          .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
          .alphaBlendOp = VK_BLEND_OP_ADD,
          .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      };

      _colorBlendAttachments.clear();
      _colorBlendAttachments.push_back(attachment);

      return *this;
    }

    GraphicsShaderComponent& enable_blending_alphablend() {
      constexpr VkPipelineColorBlendAttachmentState attachment = {
          .blendEnable = VK_TRUE,
          .srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
          .dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA,
          .colorBlendOp = VK_BLEND_OP_ADD,
          .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
          .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
          .alphaBlendOp = VK_BLEND_OP_ADD,
          .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      };

      _colorBlendAttachments.clear();
      _colorBlendAttachments.push_back(attachment);

      return *this;
    }

    GraphicsShaderComponent& set_color_attachment_format(VkFormat format) {
      _colorAttachmentformats.emplace_back(format);

      return *this;
    }

    GraphicsShaderComponent& set_color_attachment_formats(
        const std::vector<VkFormat>& formats) {
      _colorAttachmentformats.insert(_colorAttachmentformats.end(), formats.begin(), formats.end());

      return *this;
    }

    GraphicsShaderComponent& set_depth_format(const VkFormat format) {
      _renderInfo.depthAttachmentFormat = format;

      return *this;
    }

    GraphicsShaderComponent& disable_depthtest() {
      _depthStencil.depthTestEnable = VK_FALSE;
      _depthStencil.depthWriteEnable = VK_FALSE;
      _depthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
      _depthStencil.depthBoundsTestEnable = VK_FALSE;
      _depthStencil.stencilTestEnable = VK_FALSE;
      _depthStencil.front = {};
      _depthStencil.back = {};
      _depthStencil.minDepthBounds = 0.f;
      _depthStencil.maxDepthBounds = 1.f;

      return *this;
    }

    GraphicsShaderComponent& enable_depthtest(const bool depth_write_enable, const VkCompareOp op) {
      _depthStencil.depthTestEnable = VK_TRUE;
      _depthStencil.depthWriteEnable = depth_write_enable;
      _depthStencil.depthCompareOp = op;
      _depthStencil.depthBoundsTestEnable = VK_FALSE;
      _depthStencil.stencilTestEnable = VK_FALSE;
      _depthStencil.front = {};
      _depthStencil.back = {};
      _depthStencil.minDepthBounds = 0.f;
      _depthStencil.maxDepthBounds = 1.f;

      return *this;
    }

    GraphicsShaderComponent& enable_dynamic_depth_bias() {
      _dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);

      return *this;
    }

    GraphicsShaderComponent& set_pipeline_layout(const VkPipelineLayout layout) {
      _pipelineLayout = layout;

      return *this;
    }

    GraphicsShaderComponent& set_pipeline_layout(const IGpu* gpu,
                                                 const std::string_view pipeline_name,
                                                 const VkPushConstantRange push_constant_range) {
      create_pipeline_layout(gpu, pipeline_name, push_constant_range);
      return *this;
    }

  };

  struct ComputeShaderComponent : ShaderComponent {

    std::vector<VkShaderModule> shader_modules;

    ComputeShaderComponent& add_descriptor_set_layout(
        const IGpu* gpu,
        std::unordered_map<uint32, std::unordered_map<uint32, VkDescriptorSetLayoutBinding>>&&
            sets) {
      create_descriptor_layout(gpu, std::move(sets));
      return *this;
    }

    ComputeShaderComponent& add_compute_shader(const std::string&& compute_source,
                                               const IGpu* gpu) {
      shader_stages.clear();
      const auto compute_shader = get_shader(std::move(compute_source), gpu);
      shader_modules.push_back(compute_shader);
      shader_stages.push_back(
          vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, compute_shader));
      return *this;
    }

    ComputeShaderComponent& add_pipeline_layout(const IGpu* gpu,
                                                const std::string_view pipeline_name,
                                                VkPushConstantRange push_constant_range) {
      create_pipeline_layout(gpu, pipeline_name, push_constant_range);
      return *this;
    }

    void create_compute_pipeline(const IGpu* gpu) {
      if (shader_stages.size() != 1) {
        assert(!"Invalid shader stages for compute pipeline");
      }

      if (shader_stages[0].stage != VK_SHADER_STAGE_COMPUTE_BIT) {
        assert(!"Invalid shader stages for compute pipeline");
      }

      const VkComputePipelineCreateInfo pipeline_info = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
      .stage = shader_stages[0],
      .layout = get_pipeline_layout(),
      };

      VkPipeline pipeline;
      VK_CHECK(vkCreateComputePipelines(gpu->getDevice(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr,
                                        &pipeline));
      set_pipeline(pipeline);

      for (const auto& shader_module : shader_modules) {
        vkDestroyShaderModule(gpu->getDevice(), shader_module, nullptr);
      }
    }
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

    void add_push_constant(const uint32_t size, const VkShaderStageFlags stage_flags) {
      push_descriptor = PushDescriptor(size, stage_flags);
    }

    [[nodiscard]] VkPushConstantRange get_push_constant_range() const {
      return push_descriptor.get_push_constant_range();
    }

  private:
    Resources read_resources;
    Resources write_resources;
    PushDescriptor push_descriptor;
  };

  struct RenderPass {
    virtual void compile(IGpu* gpu) = 0;
    virtual std::vector<std::shared_ptr<ResourceInstance>> get_resources(ResourceUsage usage) = 0;
    virtual void execute(CommandBuffer cmd) = 0;
    virtual ~RenderPass() = default;
    std::string name;
  };

  struct DrawCullDirectionalDepthPass : RenderPass {
    // TODO
    struct alignas(16) DrawCullDepthConstants {
      int32 draw_count;
    };
    ComputeShaderComponent shader_component;
    ResourceComponent resource_component;

    DrawCullDirectionalDepthPass(const std::shared_ptr<ResourceInstance>& shadow_map,
                                 const std::shared_ptr<ResourceInstance>& geometry_buffer) {
      resource_component.add_resource(geometry_buffer, ResourceUsage::READ);
      resource_component.add_resource(shadow_map, ResourceUsage::WRITE);
      resource_component.add_push_constant(sizeof(DrawCullDepthConstants),
                                           VK_SHADER_STAGE_COMPUTE_BIT);
      name = "Shadow Map Pass";
    }

    std::vector<std::shared_ptr<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resource_component.get_resources(usage);
    }

    void compile(IGpu* gpu) override {
      shader_component
          .add_descriptor_set_layout(gpu,
                                     DescriptorLayoutBuilder()
                                         .set(0, DescriptorSetLayoutBuilder()
                                                     .binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                              VK_SHADER_STAGE_COMPUTE_BIT)
                                                     .get())
                                         .set(1, DescriptorSetLayoutBuilder()
                                                     .binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                              VK_SHADER_STAGE_COMPUTE_BIT)
                                                     .binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                              VK_SHADER_STAGE_COMPUTE_BIT)
                                                     .binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                              VK_SHADER_STAGE_COMPUTE_BIT)
                                                     .binding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                              VK_SHADER_STAGE_COMPUTE_BIT)
                                                     .binding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                              VK_SHADER_STAGE_COMPUTE_BIT)
                                                     .binding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                              VK_SHADER_STAGE_COMPUTE_BIT)
                                                     .binding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                              VK_SHADER_STAGE_COMPUTE_BIT)
                                                     .binding(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                              VK_SHADER_STAGE_COMPUTE_BIT)
                                                     .get())
                                         .get())
          .add_compute_shader("draw_cull_depth.comp.spv", gpu)
          .add_pipeline_layout(gpu, name, resource_component.get_push_constant_range())
          .create_compute_pipeline(gpu);
    }

    void execute(CommandBuffer cmd) override {
      // draw
    }
  };

  struct TaskSubmitDirectionalDepthPass : RenderPass {
    ComputeShaderComponent shader_component;
    ResourceComponent resource_component;

    TaskSubmitDirectionalDepthPass(const std::shared_ptr<ResourceInstance>& shadow_map,
                  const std::shared_ptr<ResourceInstance>& geometry_buffer) {
      resource_component.add_resource(geometry_buffer, ResourceUsage::READ);
      resource_component.add_resource(shadow_map, ResourceUsage::WRITE);
      name = "Shadow Map Pass";
    }

    std::vector<std::shared_ptr<ResourceInstance>> get_resources(ResourceUsage usage) override {
      return resource_component.get_resources(usage);
    }

    void compile(IGpu* gpu) override {
      // Specific pipeline layout and pipeline creation for compute
    }

    void execute(CommandBuffer cmd) override {
      // draw
    }
  };
  struct MeshletDirectionalDepthPass : RenderPass {
    ComputeShaderComponent shader_component;
    ResourceComponent resource_component;

    MeshletDirectionalDepthPass(const std::shared_ptr<ResourceInstance>& shadow_map,
                  const std::shared_ptr<ResourceInstance>& geometry_buffer) {
      resource_component.add_resource(geometry_buffer, ResourceUsage::READ);
      resource_component.add_resource(shadow_map, ResourceUsage::WRITE);
      name = "Shadow Map Pass";
    }

    std::vector<std::shared_ptr<ResourceInstance>> get_resources(ResourceUsage usage) override {
      return resource_component.get_resources(usage);
    }

    void compile(IGpu* gpu) override {
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
      name = "Geometry Pass";
    }

    void compile(IGpu* gpu) override {
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
      name = "Lighting Pass";
    }

    std::vector<std::shared_ptr<ResourceInstance>> get_resources(ResourceUsage usage) override {
      return resource_component.get_resources(usage);
    }

    void compile(IGpu* gpu) override {
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
      name = "Ssao Pass";
    }

    std::vector<std::shared_ptr<ResourceInstance>> get_resources(ResourceUsage usage) override {
      return resource_component.get_resources(usage);
    }

    void compile(IGpu* gpu) override {
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
      name = "Tone Map Pass";
    }

    std::vector<std::shared_ptr<ResourceInstance>> get_resources(ResourceUsage usage) override {
      return resource_component.get_resources(usage);
    }

    void compile(IGpu* gpu) override {
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
    std::unordered_map<uint64, std::shared_ptr<FrameGraphEdge>> edges_;

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

    void compile(IGpu* gpu);

    void execute(CommandBuffer cmd);
  };

}  // namespace gestalt::graphics::fg