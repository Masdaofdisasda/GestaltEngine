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

    DescriptorSetLayoutBuilder& binding(uint32_t binding_index, const VkDescriptorType descriptor_type,
                                        VkShaderStageFlags shader_stages,
                                        const uint32_t descriptor_count = 1) {
      if (auto [_, inserted]
          = bindings.emplace(binding_index, VkDescriptorSetLayoutBinding{
                                 .binding = binding_index,
                                 .descriptorType = descriptor_type,
                                 .descriptorCount = descriptor_count,
                                 .stageFlags = shader_stages,
                             }); !inserted) {
        throw std::runtime_error("Binding index " + std::to_string(binding_index)
                                 + " already exists in DescriptorSetLayoutBuilder.");
      }

      return *this;
    }

    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> get() { return std::move(bindings); }
  };

  struct DescriptorLayoutBuilder {
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>>
        set_bindings;

    DescriptorLayoutBuilder& set(
        uint32_t set_index, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>&& bindings) {
      if (auto [_, inserted] = set_bindings.emplace(set_index, std::move(bindings)); !inserted) {
        throw std::runtime_error("Set index " + std::to_string(set_index)
                                 + " already exists in DescriptorLayoutBuilder.");
      }

      return *this;
    }

    std::unordered_map<uint32_t, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>> get() {
      return std::move(set_bindings);
    }
  };

  class PipelineUtil : NonCopyable<PipelineUtil> {
    IGpu* gpu_ = nullptr;

    std::unordered_map<uint32, std::unordered_map<uint32, VkDescriptorSetLayoutBinding>>
        set_bindings_;
    VkPipelineLayout pipeline_layout_ = nullptr;
    VkPipeline pipeline_ = nullptr;
    std::unordered_map<uint32, VkDescriptorSetLayout> descriptor_set_layouts_;
    std::unordered_map<VkShaderStageFlagBits, VkPipelineShaderStageCreateInfo> shader_stages_;
    std::unordered_map<VkShaderStageFlagBits, VkShaderModule> shader_modules_;

  public:
    explicit PipelineUtil(IGpu* gpu) : gpu_(gpu) {}

    ~PipelineUtil() = default;

    void create_graphics_pipeline(VkGraphicsPipelineCreateInfo pipeline_create_info,
                                  const std::vector<VkShaderStageFlagBits>& shader_types,
                                  const std::string_view pipeline_name) {

      for (const auto& shader_type : shader_types) {
        assert(shader_stages_.contains(shader_type) && "Missing shader stage");
      }

      std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
      for (const auto& shader_stage : shader_stages_ | std::views::values) {
        shader_stages.push_back(shader_stage);
      }

      pipeline_create_info.stageCount = static_cast<uint32>(shader_stages.size());
      pipeline_create_info.pStages = shader_stages.data();
      pipeline_create_info.layout = pipeline_layout_;

      VK_CHECK(vkCreateGraphicsPipelines(gpu_->getDevice(), VK_NULL_HANDLE, 1,
                                         &pipeline_create_info, nullptr, &pipeline_));

      gpu_->set_debug_name(pipeline_name, VK_OBJECT_TYPE_PIPELINE,
                           reinterpret_cast<uint64_t>(pipeline_));

      for (const auto& shader_module : shader_modules_ | std::views::values) {
        vkDestroyShaderModule(gpu_->getDevice(), shader_module, nullptr);
      }
    }

    void create_compute_pipeline(const std::string_view pipeline_name) {
      assert(shader_stages_.contains(VK_SHADER_STAGE_COMPUTE_BIT) && "No compute shader added");

      const VkComputePipelineCreateInfo pipeline_info = {
          .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
          .pNext = nullptr,
          .flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
          .stage = shader_stages_.at(VK_SHADER_STAGE_COMPUTE_BIT),
          .layout = pipeline_layout_,
      };

      VK_CHECK(vkCreateComputePipelines(gpu_->getDevice(), VK_NULL_HANDLE, 1, &pipeline_info,
                                        nullptr, &pipeline_));

      gpu_->set_debug_name(pipeline_name, VK_OBJECT_TYPE_PIPELINE,
                           reinterpret_cast<uint64_t>(pipeline_));

      for (const auto& shader_module : shader_modules_ | std::views::values) {
        vkDestroyShaderModule(gpu_->getDevice(), shader_module, nullptr);
      }
    }

    void add_shader(const std::string&& source_file, VkShaderStageFlagBits shader_stage) {
      const std::string& shader_path = "../shaders/" + source_file;

      VkShaderModule shader_module;
      vkutil::load_shader_module(shader_path.c_str(), gpu_->getDevice(), &shader_module);
      shader_modules_.emplace(shader_stage, shader_module);

      gpu_->set_debug_name(source_file, VK_OBJECT_TYPE_SHADER_MODULE,
                           reinterpret_cast<uint64_t>(shader_module));

      vkinit::pipeline_shader_stage_create_info(shader_stage, shader_module);
      shader_stages_.emplace(
          shader_stage, vkinit::pipeline_shader_stage_create_info(shader_stage, shader_module));
    }

    void create_descriptor_layout(
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
        VK_CHECK(vkCreateDescriptorSetLayout(gpu_->getDevice(), &info, nullptr, &set));
        descriptor_set_layouts_.emplace(set_index, set);
      }
    }

    void create_pipeline_layout(const std::string_view pipeline_name,
                                const VkPushConstantRange push_constant_range) {
      std::vector<VkDescriptorSetLayout> descriptor_set_layouts_vector;
      for (const auto& layout : descriptor_set_layouts_ | std::views::values) {
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
      VK_CHECK(vkCreatePipelineLayout(gpu_->getDevice(), &pipeline_layout_create_info, nullptr,
                                      &pipeline_layout));
      pipeline_layout_ = pipeline_layout;

      gpu_->set_debug_name(pipeline_name, VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                           reinterpret_cast<uint64_t>(pipeline_layout_));
    }
  };

    class GraphicsPipelineBuilder {
    VkPipelineInputAssemblyStateCreateInfo input_assembly_{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    VkPipelineRasterizationStateCreateInfo rasterizer_{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachments_;
    VkPipelineMultisampleStateCreateInfo multisampling_{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    VkPipelineDepthStencilStateCreateInfo depth_stencil_{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    VkPipelineRenderingCreateInfo render_info_{.sType
                                               = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    std::vector<VkFormat> color_attachmentformats_;
    std::vector<VkDynamicState> dynamic_states_
        = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamic_info_
        = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};

      VkPipelineViewportStateCreateInfo viewport_state_
        = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    VkPipelineColorBlendStateCreateInfo color_blending_ = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    };

      // completely clear VertexInputStateCreateInfo, as we have no need for it
      VkPipelineVertexInputStateCreateInfo vertex_input_info_
          = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    VkGraphicsPipelineCreateInfo pipeline_info_
        = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};

    public:


      GraphicsPipelineBuilder& set_depth_format(const VkFormat format) {
          render_info_.depthAttachmentFormat = format;

        return *this;
      }

      GraphicsPipelineBuilder& set_input_topology(const VkPrimitiveTopology topology) {
        input_assembly_.topology = topology;
        input_assembly_.primitiveRestartEnable = VK_FALSE;

        return *this;
      }

      GraphicsPipelineBuilder& set_polygon_mode(const VkPolygonMode mode) {
        rasterizer_.polygonMode = mode;
        rasterizer_.lineWidth = 1.f;

        return *this;
      }

      GraphicsPipelineBuilder& set_cull_mode(const VkCullModeFlags cull_mode,
                                      const VkFrontFace front_face) {
        rasterizer_.cullMode = cull_mode;
        rasterizer_.frontFace = front_face;

        return *this;
      }

      GraphicsPipelineBuilder& set_multisampling_none() {
        multisampling_.sampleShadingEnable = VK_FALSE;
        // multisampling defaulted to no multisampling (1 sample per pixel)
        multisampling_.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling_.minSampleShading = 1.0f;
        multisampling_.pSampleMask = nullptr;
        // no alpha to coverage either
        multisampling_.alphaToCoverageEnable = VK_FALSE;
        multisampling_.alphaToOneEnable = VK_FALSE;

        return *this;
      }

      GraphicsPipelineBuilder& disable_blending(uint32_t count = 1) {
        constexpr VkPipelineColorBlendAttachmentState attachment = {
            .blendEnable = VK_FALSE,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                              | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };

        for (uint32_t i = 0; i < count; i++) {
          color_blend_attachments_.push_back(attachment);
        }

        return *this;
      }

      GraphicsPipelineBuilder& enable_blending_additive() {
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

        color_blend_attachments_.clear();
        color_blend_attachments_.push_back(attachment);

        return *this;
      }

      GraphicsPipelineBuilder& enable_blending_alphablend() {
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

        color_blend_attachments_.clear();
        color_blend_attachments_.push_back(attachment);

        return *this;
      }

      GraphicsPipelineBuilder& set_color_attachment_format(VkFormat format) {
        color_attachmentformats_.emplace_back(format);

        return *this;
      }

      GraphicsPipelineBuilder& set_color_attachment_formats(
          const std::vector<VkFormat>& formats) {
        color_attachmentformats_.insert(color_attachmentformats_.end(), formats.begin(),
                                        formats.end());

        return *this;
      }

      GraphicsPipelineBuilder& disable_depthtest() {
        depth_stencil_.depthTestEnable = VK_FALSE;
        depth_stencil_.depthWriteEnable = VK_FALSE;
        depth_stencil_.depthCompareOp = VK_COMPARE_OP_NEVER;
        depth_stencil_.depthBoundsTestEnable = VK_FALSE;
        depth_stencil_.stencilTestEnable = VK_FALSE;
        depth_stencil_.front = {};
        depth_stencil_.back = {};
        depth_stencil_.minDepthBounds = 0.f;
        depth_stencil_.maxDepthBounds = 1.f;

        return *this;
      }

      GraphicsPipelineBuilder& enable_depthtest(const bool depth_write_enable,
                                                const VkCompareOp op) {
        depth_stencil_.depthTestEnable = VK_TRUE;
        depth_stencil_.depthWriteEnable = depth_write_enable;
        depth_stencil_.depthCompareOp = op;
        depth_stencil_.depthBoundsTestEnable = VK_FALSE;
        depth_stencil_.stencilTestEnable = VK_FALSE;
        depth_stencil_.front = {};
        depth_stencil_.back = {};
        depth_stencil_.minDepthBounds = 0.f;
        depth_stencil_.maxDepthBounds = 1.f;

        return *this;
      }

      GraphicsPipelineBuilder& enable_dynamic_depth_bias() {
        dynamic_states_.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);

        return *this;
      }

      VkGraphicsPipelineCreateInfo build_pipeline_info() {
        viewport_state_.pNext = nullptr;
        viewport_state_.viewportCount = 1;
        viewport_state_.scissorCount = 1;

        color_blending_.pNext = nullptr;
        color_blending_.logicOpEnable = VK_FALSE;
        color_blending_.logicOp = VK_LOGIC_OP_COPY;
        color_blending_.attachmentCount = color_blend_attachments_.size();
        color_blending_.pAttachments = color_blend_attachments_.data();

        dynamic_info_.pDynamicStates = dynamic_states_.data();
        dynamic_info_.dynamicStateCount = dynamic_states_.size();

        render_info_.colorAttachmentCount = color_attachmentformats_.size();
        render_info_.pColorAttachmentFormats = color_attachmentformats_.data();

        pipeline_info_.pNext = &render_info_;
        pipeline_info_.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
        pipeline_info_.pVertexInputState = &vertex_input_info_;
        pipeline_info_.pInputAssemblyState = &input_assembly_;
        pipeline_info_.pViewportState = &viewport_state_;
        pipeline_info_.pRasterizationState = &rasterizer_;
        pipeline_info_.pMultisampleState = &multisampling_;
        pipeline_info_.pColorBlendState = &color_blending_;
        pipeline_info_.pDepthStencilState = &depth_stencil_;
        pipeline_info_.pDynamicState = &dynamic_info_;

        return pipeline_info_;
      }

    };

  class CreationStageGuard {
      enum class CreationStage {
        kDescriptorLayout,
        kShader,
        kPipelineLayout,
        kPipeline
      } stage_ = CreationStage::kDescriptorLayout;

    void is_stage(const CreationStage stage) const {
      if (stage_ != stage) {
        throw std::runtime_error("Invalid stage");
      }
    }
  public:

    void in_descriptor_stage() const { is_stage(CreationStage::kDescriptorLayout); }
    void in_shader_stage() const { is_stage(CreationStage::kShader); }
    void in_pipeline_layout_stage() const { is_stage(CreationStage::kPipelineLayout); }
    void in_pipeline_stage() const { is_stage(CreationStage::kPipeline); }

    void next_stage() {
      switch (stage_) {
        case CreationStage::kDescriptorLayout:
          stage_ = CreationStage::kShader;
          break;
        case CreationStage::kShader:
          stage_ = CreationStage::kPipelineLayout;
          break;
        case CreationStage::kPipelineLayout:
          stage_ = CreationStage::kPipeline;
          break;
        case CreationStage::kPipeline:
          throw std::runtime_error("Invalid stage");
      }
    }
  };

  class GraphicsPipeline {
    PipelineUtil util_;
    CreationStageGuard guard_;
    GraphicsPipelineBuilder graphics_pipeline_builder_{};
    std::vector<VkShaderStageFlagBits> shader_types_;


  public:

    explicit GraphicsPipeline(IGpu* gpu) : util_(gpu) {}

    GraphicsPipeline& add_descriptor_set_layout(
        std::unordered_map<uint32, std::unordered_map<uint32, VkDescriptorSetLayoutBinding>>&&
            sets) {
      guard_.in_descriptor_stage();
      util_.create_descriptor_layout(std::move(sets));
      guard_.next_stage();
      return *this;
    }

    GraphicsPipeline& set_vertex_shading(const std::string&& vertex_source,
                                         const std::string&& fragment_source) {
      guard_.in_shader_stage();
      util_.add_shader(std::move(vertex_source), VK_SHADER_STAGE_VERTEX_BIT);
      util_.add_shader(std::move(fragment_source), VK_SHADER_STAGE_FRAGMENT_BIT);

      shader_types_.push_back(VK_SHADER_STAGE_VERTEX_BIT);
      shader_types_.push_back(VK_SHADER_STAGE_FRAGMENT_BIT);
      guard_.next_stage();

      return *this;
    }

    GraphicsPipeline& set_mesh_task_shading(const std::string&& task_source,
                                            const std::string&& mesh_source,
                                            const std::string&& fragment_source) {
      guard_.in_shader_stage();
      util_.add_shader(std::move(task_source), VK_SHADER_STAGE_TASK_BIT_EXT);
      util_.add_shader(std::move(mesh_source), VK_SHADER_STAGE_MESH_BIT_EXT);
      util_.add_shader(std::move(fragment_source), VK_SHADER_STAGE_FRAGMENT_BIT);

      shader_types_.push_back(VK_SHADER_STAGE_TASK_BIT_EXT);
      shader_types_.push_back(VK_SHADER_STAGE_MESH_BIT_EXT);
      shader_types_.push_back(VK_SHADER_STAGE_FRAGMENT_BIT);
      guard_.next_stage();

      return *this;
    }

    GraphicsPipeline& add_pipeline_layout(const std::string_view pipeline_name,
                                         VkPushConstantRange push_constant_range) {
      guard_.in_pipeline_layout_stage();
      util_.create_pipeline_layout(pipeline_name, push_constant_range);
      guard_.next_stage();
      return *this;
    }

    void create_graphics_pipeline(const VkGraphicsPipelineCreateInfo& pipeline_create_info,
                                  const std::string_view pipeline_name) {
      guard_.in_pipeline_stage();
      util_.create_graphics_pipeline(pipeline_create_info, shader_types_, pipeline_name);
    }

  };

  class ComputePipeline {
    PipelineUtil util_;
    CreationStageGuard guard_;

  public:
    explicit ComputePipeline(IGpu* gpu) : util_(gpu) {}

    ComputePipeline& add_descriptor_set_layout(
        std::unordered_map<uint32, std::unordered_map<uint32, VkDescriptorSetLayoutBinding>>&&
            sets) {
      guard_.in_descriptor_stage();
      util_.create_descriptor_layout(std::move(sets));
      guard_.next_stage();
      return *this;
    }

    ComputePipeline& add_compute_shader(const std::string&& compute_source) {
      guard_.in_shader_stage();
      util_.add_shader(std::move(compute_source), VK_SHADER_STAGE_COMPUTE_BIT);
      guard_.next_stage();
      return *this;
    }

    ComputePipeline& add_pipeline_layout(const std::string_view pipeline_name,
                                         VkPushConstantRange push_constant_range) {
      guard_.in_pipeline_layout_stage();
      util_.create_pipeline_layout(pipeline_name, push_constant_range);
      guard_.next_stage();
      return *this;
    }

    void create_compute_pipeline(const std::string_view pipeline_name) {
      guard_.in_pipeline_stage();
      util_.create_compute_pipeline(pipeline_name);
    }
  };

  struct Resources {
    std::vector<std::shared_ptr<ResourceInstance>> resources;
  };

  enum class BindingType {
    DESCRIPTOR,
    COLOR_ATTACHMENT,
    DEPTH_ATTACHMENT,
  };

  struct ResourceBinding {
    BindingType type;
    uint32_t setIndex = 0;         // Descriptor set index for DESCRIPTOR
    uint32_t bindingIndex = 0;     // Binding index for DESCRIPTOR
    uint32_t attachmentIndex = 0;  // Attachment index for COLOR/DEPTH_ATTACHMENT
    VkImageLayout layout
        = VK_IMAGE_LAYOUT_UNDEFINED;  // Optional: Track image layout for attachments
  };


  enum class ResourceUsage { READ, WRITE };

  class ResourceComponent {
  public:
    void add_resource(const std::shared_ptr<ResourceInstance>& resource, const ResourceUsage usage,
                      const ResourceBinding& binding) {
      resources_[resource] = {usage, binding};
    }

    std::vector<std::shared_ptr<ResourceInstance>> get_resources(const ResourceUsage usage) const {
      std::vector<std::shared_ptr<ResourceInstance>> result;
      for (const auto& [resource, metadata] : resources_) {
        if (metadata.usage == usage) {
          result.push_back(resource);
        }
      }
      return result;
    }

    const ResourceBinding* get_binding(const std::shared_ptr<ResourceInstance>& resource) const {
      auto it = resources_.find(resource);
      return (it != resources_.end()) ? &it->second.binding : nullptr;
    }

    void add_push_constant(uint32_t size, VkShaderStageFlags stage_flags) {
      push_descriptor_ = PushDescriptor(size, stage_flags);
    }

    [[nodiscard]] VkPushConstantRange get_push_constant_range() const {
      return push_descriptor_.get_push_constant_range();
    }

  private:
    struct ResourceMetadata {
      ResourceUsage usage;
      ResourceBinding binding;
    };

    std::unordered_map<std::shared_ptr<ResourceInstance>, ResourceMetadata> resources_;
    PushDescriptor push_descriptor_;
  };

  struct RenderPass {
    virtual void compile() = 0;
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
    ComputePipeline compute_pipeline;
    ResourceComponent resource_component;

    DrawCullDirectionalDepthPass(const std::shared_ptr<BufferInstance>& camera_buffer,
                                 const std::shared_ptr<BufferInstance>& task_commands,
                                 const std::shared_ptr<BufferInstance>& draws,
                                 const std::shared_ptr<BufferInstance>& command_count,
                                 IGpu* gpu)
        : compute_pipeline(gpu) {
      resource_component.add_resource(camera_buffer, ResourceUsage::READ, {BindingType::DESCRIPTOR, 0, 0});
      resource_component.add_resource(draws, ResourceUsage::READ, {BindingType::DESCRIPTOR, 1, 6});
      resource_component.add_resource(task_commands, ResourceUsage::WRITE,
                                      {BindingType::DESCRIPTOR, 1, 5});
      resource_component.add_resource(command_count, ResourceUsage::WRITE,
                                      {BindingType::DESCRIPTOR, 1, 7});
      resource_component.add_push_constant(sizeof(DrawCullDepthConstants),
                                           VK_SHADER_STAGE_COMPUTE_BIT);
      name = "Draw Cull Directional Depth Pass";
    }

    std::vector<std::shared_ptr<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resource_component.get_resources(usage);
    }

    void compile() override {
      compute_pipeline
          .add_descriptor_set_layout(DescriptorLayoutBuilder()
                                         .set(0, DescriptorSetLayoutBuilder()
                                                     .binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                              VK_SHADER_STAGE_COMPUTE_BIT)
                                                     .get())
                                         .set(1, DescriptorSetLayoutBuilder()
                                                     .binding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                              VK_SHADER_STAGE_COMPUTE_BIT)
                                                     .binding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                              VK_SHADER_STAGE_COMPUTE_BIT)
                                                     .binding(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                              VK_SHADER_STAGE_COMPUTE_BIT)
                                                     .get())
                                         .get())
          .add_compute_shader("draw_cull_depth.comp.spv")
          .add_pipeline_layout(name, resource_component.get_push_constant_range())
          .create_compute_pipeline(name);
    }

    void execute(CommandBuffer cmd) override {
      // draw
    }
  };

  struct TaskSubmitDirectionalDepthPass : RenderPass {
    ComputePipeline compute_pipeline;
    ResourceComponent resource_component;

    TaskSubmitDirectionalDepthPass(const std::shared_ptr<BufferInstance>& task_commands,
                                   const std::shared_ptr<BufferInstance>& command_count,
                                   IGpu* gpu)
        : compute_pipeline(gpu) {
      resource_component.add_resource(task_commands, ResourceUsage::READ,
                                      {BindingType::DESCRIPTOR, 0, 5});
      resource_component.add_resource(task_commands, ResourceUsage::WRITE,
                                      {BindingType::DESCRIPTOR, 0, 5});
      resource_component.add_resource(command_count, ResourceUsage::WRITE,
                                      {BindingType::DESCRIPTOR, 0, 7});
      name = "Task Submit Directional Depth Pass";
    }

    std::vector<std::shared_ptr<ResourceInstance>> get_resources(ResourceUsage usage) override {
      return resource_component.get_resources(usage);
    }

    void compile() override {
      compute_pipeline
          .add_descriptor_set_layout(DescriptorLayoutBuilder()
                                         .set(0, DescriptorSetLayoutBuilder()
                                                     .binding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                              VK_SHADER_STAGE_COMPUTE_BIT)
                                                     .binding(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                              VK_SHADER_STAGE_COMPUTE_BIT)
                                                     .get())
                                         .get())
          .add_compute_shader("task_submit.comp.spv")
          .add_pipeline_layout(name, resource_component.get_push_constant_range())
          .create_compute_pipeline(name);
    }

    void execute(CommandBuffer cmd) override {
      // draw
    }
  };
  struct MeshletDirectionalDepthPass : RenderPass {
    GraphicsPipeline graphics_pipeline;
    ResourceComponent resources;

    MeshletDirectionalDepthPass(const std::shared_ptr<BufferInstance>& camera_buffer,
                                const std::shared_ptr<BufferInstance>& light_matrices,
                                const std::shared_ptr<BufferInstance>& directional_light,
                                const std::shared_ptr<BufferInstance>& point_light,
                                const std::shared_ptr<BufferInstance>& vertex_positions,
                                const std::shared_ptr<BufferInstance>& vertex_data,
                                const std::shared_ptr<BufferInstance>& meshlet,
                                const std::shared_ptr<BufferInstance>& meshlet_vertices,
                                const std::shared_ptr<BufferInstance>& meshlet_indices,
                                const std::shared_ptr<BufferInstance>& task_commands,
                                const std::shared_ptr<BufferInstance>& draws,
                                const std::shared_ptr<ImageInstance>& shadow_map, IGpu* gpu)
        : graphics_pipeline(gpu) {
      resources.add_resource(camera_buffer, ResourceUsage::READ, {BindingType::DESCRIPTOR, 0, 0});

      resources.add_resource(light_matrices, ResourceUsage::READ, {BindingType::DESCRIPTOR, 1, 0});
      resources.add_resource(directional_light, ResourceUsage::READ,
                             {BindingType::DESCRIPTOR, 1, 1});
      resources.add_resource(point_light, ResourceUsage::READ, {BindingType::DESCRIPTOR, 1, 2});

      resources.add_resource(vertex_positions, ResourceUsage::READ,
                             {BindingType::DESCRIPTOR, 2, 0});
      resources.add_resource(vertex_data, ResourceUsage::READ, {BindingType::DESCRIPTOR, 2, 1});
      resources.add_resource(meshlet, ResourceUsage::READ, {BindingType::DESCRIPTOR, 2, 2});
      resources.add_resource(meshlet_vertices, ResourceUsage::READ,
                             {BindingType::DESCRIPTOR, 2, 3});
      resources.add_resource(meshlet_indices, ResourceUsage::READ, {BindingType::DESCRIPTOR, 2, 4});
      resources.add_resource(task_commands, ResourceUsage::READ,
                             {BindingType::DEPTH_ATTACHMENT, 2, 5});
      resources.add_resource(draws, ResourceUsage::READ, {BindingType::DEPTH_ATTACHMENT, 2, 6});
      resources.add_resource(shadow_map, ResourceUsage::WRITE, {BindingType::DEPTH_ATTACHMENT});
      name = "Meshlet Directional Depth Pass";
    }

    std::vector<std::shared_ptr<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources.get_resources(usage);
    }

    void compile() override {
      auto write = resources.get_resources(ResourceUsage::WRITE);

      GraphicsPipelineBuilder pipeline_builder{};
      pipeline_builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
          .set_polygon_mode(VK_POLYGON_MODE_FILL)
          .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
          .set_multisampling_none()
          .disable_blending()
          .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
          .set_depth_format(VK_FORMAT_D32_SFLOAT);

      graphics_pipeline
          .add_descriptor_set_layout(
              DescriptorLayoutBuilder()
                  .set(0, DescriptorSetLayoutBuilder()
                              .binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                       VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT
                                           | VK_SHADER_STAGE_FRAGMENT_BIT)
                              .get())
                  .set(1, DescriptorSetLayoutBuilder()
                              .binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       VK_SHADER_STAGE_MESH_BIT_EXT)
                              .binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       VK_SHADER_STAGE_MESH_BIT_EXT)
                              .binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       VK_SHADER_STAGE_MESH_BIT_EXT)
                              .get())
                  .set(2, DescriptorSetLayoutBuilder()
                              .binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                              .binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                              .binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                              .binding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                              .binding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                              .binding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                              .binding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                              .get())
                  .get())
          .set_mesh_task_shading("geometry_depth.task.spv", "geometry_depth.mesh.spv",
                                 "geometry_depth.frag.spv")
          .add_pipeline_layout(name, resources.get_push_constant_range())
          .create_graphics_pipeline(pipeline_builder.build_pipeline_info(), name);
    }

    void execute(CommandBuffer cmd) override {
      // draw
    }
  };

  struct GeometryPass : RenderPass {
    GraphicsPipeline graphics_pipeline;
    ResourceComponent resource_component;

    std::vector<std::shared_ptr<ResourceInstance>> get_resources(ResourceUsage usage) override {
      return resource_component.get_resources(usage);
    }

    GeometryPass(const std::shared_ptr<ImageInstance>& g_buffer_1,
                 const std::shared_ptr<ImageInstance>& g_buffer_2,
                 const std::shared_ptr<ImageInstance>& g_buffer_3,
                 const std::shared_ptr<ImageInstance>& g_buffer_depth,
                 const std::shared_ptr<BufferInstance>& geometry_buffer, IGpu* gpu)
        : graphics_pipeline(gpu) {
      resource_component.add_resource(geometry_buffer, ResourceUsage::READ,
                                      {BindingType::DESCRIPTOR, 0, 0});
      resource_component.add_resource(g_buffer_1, ResourceUsage::WRITE,
                                      {BindingType::DESCRIPTOR, 0, 0});
      resource_component.add_resource(g_buffer_2, ResourceUsage::WRITE,
                                      {BindingType::DESCRIPTOR, 0, 0});
      resource_component.add_resource(g_buffer_3, ResourceUsage::WRITE,
                                      {BindingType::DESCRIPTOR, 0, 0});
      resource_component.add_resource(g_buffer_depth, ResourceUsage::WRITE,
                                      {BindingType::DESCRIPTOR, 0, 0});
      name = "Geometry Pass";
    }

    void compile() override {
      // Specific pipeline layout and pipeline creation for graphics
    }

    void execute(CommandBuffer cmd) override {
      // draw
    }
  };

  struct LightingPass : RenderPass {
    ComputePipeline compute_pipeline;
    ResourceComponent resource_component;

    LightingPass(const std::shared_ptr<ResourceInstance>& scene_lit,
                 const std::shared_ptr<ResourceInstance>& g_buffer_1,
                 const std::shared_ptr<ResourceInstance>& g_buffer_2,
                 const std::shared_ptr<ResourceInstance>& g_buffer_3,
                 const std::shared_ptr<ResourceInstance>& g_buffer_depth,
                 const std::shared_ptr<ResourceInstance>& shadow_map,
                 const std::shared_ptr<ResourceInstance>& material_buffer,
                 const std::shared_ptr<ResourceInstance>& light_buffer, IGpu* gpu)
        : compute_pipeline(gpu) {
      resource_component.add_resource(scene_lit, ResourceUsage::WRITE, {});
      resource_component.add_resource(g_buffer_1, ResourceUsage::READ,
                                      {});
      resource_component.add_resource(g_buffer_2, ResourceUsage::READ, {});
      resource_component.add_resource(g_buffer_3, ResourceUsage::READ, {});
      resource_component.add_resource(g_buffer_depth, ResourceUsage::READ, {});
      resource_component.add_resource(shadow_map, ResourceUsage::READ, {});
      resource_component.add_resource(material_buffer, ResourceUsage::READ, {});
      resource_component.add_resource(light_buffer, ResourceUsage::READ, {});
      name = "Lighting Pass";
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
    ComputePipeline compute_pipeline;
    ResourceComponent resource_component;

    SsaoPass(const std::shared_ptr<ResourceInstance>& g_buffer_depth,
             const std::shared_ptr<ResourceInstance>& g_buffer_2,
             const std::shared_ptr<ResourceInstance>& rotation_texture,
             const std::shared_ptr<ResourceInstance>& occlusion, IGpu* gpu)
        : compute_pipeline(gpu) {
      resource_component.add_resource(g_buffer_depth, ResourceUsage::READ, {});
      resource_component.add_resource(g_buffer_2, ResourceUsage::READ, {});
      resource_component.add_resource(rotation_texture, ResourceUsage::READ, {});
      resource_component.add_resource(occlusion, ResourceUsage::WRITE, {});
      name = "Ssao Pass";
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
    ComputePipeline compute_pipeline;
    ResourceComponent resource_component;

    ToneMapPass(const std::shared_ptr<ResourceInstance>& scene_final,
                const std::shared_ptr<ResourceInstance>& scene_lit, IGpu* gpu)
        : compute_pipeline(gpu) {
      resource_component.add_resource(scene_lit, ResourceUsage::READ, {});
      resource_component.add_resource(scene_final, ResourceUsage::WRITE, {});
      name = "Tone Map Pass";
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
    explicit ResourceRegistry(ResourceAllocator* resource_factory)
        : resource_factory_(resource_factory) {}

    std::shared_ptr<ImageInstance> add_template(ImageTemplate&& image_template);

    std::shared_ptr<BufferInstance> add_template(BufferTemplate&& buffer_template);

    template <typename ResourceInstanceType>
    std::shared_ptr<ResourceInstanceType> add_resource(
        std::shared_ptr<ResourceInstanceType> resource_instance);

    std::shared_ptr<ResourceInstance> get_resource(const uint32 handle) {
      return resource_map_.at(handle);
    }
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
    explicit FrameGraph(ResourceAllocator* resource_allocator);

    void add_render_pass(std::shared_ptr<RenderPass>&& pass);

    std::shared_ptr<ImageInstance> add_resource(ImageTemplate&& image_template,
                                                CreationType creation_type
                                                = CreationType::INTERNAL) {
      auto resource = resource_registry_->add_template(std::move(image_template));
      const uint64 handle = resource->resource_handle;
      assert(handle != 0 && "Invalid resource handle!");

      auto inserted = edges_.emplace(
          handle, std::make_shared<FrameGraphEdge>(std::move(resource), creation_type));
      assert(inserted.second && "Failed to insert edge into edges map!");

      return std::static_pointer_cast<ImageInstance>(inserted.first->second->resource);
    }

    std::shared_ptr<BufferInstance> add_resource(BufferTemplate&& buffer_template,
                                                 CreationType creation_type
                                                 = CreationType::INTERNAL) {
      auto resource = resource_registry_->add_template(std::move(buffer_template));
      const uint64 handle = resource->resource_handle;
      assert(handle != 0 && "Invalid resource handle!");

      auto inserted = edges_.emplace(
          handle, std::make_shared<FrameGraphEdge>(std::move(resource), creation_type));
      assert(inserted.second && "Failed to insert edge into edges map!");

      return std::static_pointer_cast<BufferInstance>(inserted.first->second->resource);
    }


    template <typename ResourceInstanceType>
    auto add_resource(std::shared_ptr<ResourceInstanceType> resource_instance,
                      CreationType creation_type = CreationType::EXTERNAL) {
      assert(resource_instance != nullptr && "Resource instance cannot be null!");

      auto resource = resource_registry_->add_resource(resource_instance);
      assert(resource != nullptr && "Failed to add resource to the registry!");

      uint64 handle = resource->resource_handle;
      assert(handle != -1 && "Invalid resource handle!");

      auto inserted = edges_.emplace(
          handle, std::make_shared<FrameGraphEdge>(std::move(resource), creation_type));
      assert(inserted.second && "Failed to insert edge into edges map!");

      return std::static_pointer_cast<ResourceInstanceType>(inserted.first->second->resource);
    }


    void compile();

    void execute(CommandBuffer cmd);
  };

}  // namespace gestalt::graphics::fg