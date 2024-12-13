#pragma once
#include <array>
#include <bitset>
#include <memory>
#include <ranges>
#include <unordered_map>
#include <utility>
#include <vector>

#include "RenderConfig.hpp"
#include "ResourceTypes.hpp"
#include "VulkanCheck.hpp"
#include "common.hpp"
#include "vk_initializers.hpp"
#include "Interface/IGpu.hpp"
#include "Resources/ResourceTypes.hpp"
#include "Utils/vk_pipelines.hpp"
#include <algorithm>

#include "PerFrameData.hpp"

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

  
    struct ResourceBindingInfo {
    uint32 set_index;
    uint32 binding_index;
    VkDescriptorType descriptor_type;
    VkShaderStageFlags shader_stages;
    uint32 descriptor_count;
    std::optional<VkSampler> sampler;
  };

  template <typename ResourceInstanceType> struct ResourceBinding {
    std::shared_ptr<ResourceInstanceType> resource;
    ResourceBindingInfo info;
    ResourceUsage usage;
  };

  class CommandBuffer {
    VkCommandBuffer cmd;

  public:
    explicit CommandBuffer(const VkCommandBuffer cmd) : cmd(cmd) {}
    // todo : add more command buffer functions and remove the get function
    VkCommandBuffer get() const { return cmd; }

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
    std::string_view pipeline_name_;

    std::unordered_map<uint32, std::unordered_map<uint32, VkDescriptorSetLayoutBinding>>
        set_bindings_;

    std::unordered_map<uint32, std::shared_ptr<DescriptorBufferInstance>> descriptor_buffers_;
    VkPipelineLayout pipeline_layout_ = nullptr;
    VkPipeline pipeline_ = nullptr;
    std::unordered_map<uint32, VkDescriptorSetLayout> descriptor_set_layouts_;
    std::unordered_map<VkShaderStageFlagBits, VkPipelineShaderStageCreateInfo> shader_stages_;
    std::unordered_map<VkShaderStageFlagBits, VkShaderModule> shader_modules_;

    void create_descriptor_layout(
        std::unordered_map<uint32, std::unordered_map<uint32, VkDescriptorSetLayoutBinding>>&&
            sets) {
      this->set_bindings_ = std::move(sets);

      // pipeline layout expects descriptor set layouts to be sorted by set index
      std::vector<uint32_t> sorted_keys;
      sorted_keys.reserve(set_bindings_.size());
      for (const auto& set_index : set_bindings_ | std::views::keys) {
        sorted_keys.push_back(set_index);
      }
      std::ranges::sort(sorted_keys);

      for (const auto& set_index : sorted_keys) {
        const auto& bindings = set_bindings_.at(set_index);
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

        VkDescriptorSetLayout descriptor_set_layout;
        VK_CHECK(vkCreateDescriptorSetLayout(gpu_->getDevice(), &info, nullptr, &descriptor_set_layout));
        const std::string descriptor_set_layout_name = std::string(pipeline_name_) + " Set "
                                                       + std::to_string(set_index)
                                                       + " Descriptor Set Layout";
        gpu_->set_debug_name(descriptor_set_layout_name, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                             reinterpret_cast<uint64_t>(descriptor_set_layout));
        descriptor_set_layouts_.emplace(set_index, descriptor_set_layout);
      }
    }

  public:
    explicit PipelineUtil(IGpu* gpu, const std::string_view pipeline_name) : gpu_(gpu), pipeline_name_(pipeline_name) {}

    ~PipelineUtil() = default;

    void create_graphics_pipeline(VkGraphicsPipelineCreateInfo pipeline_create_info,
                                  const std::vector<VkShaderStageFlagBits>& shader_types) {

      for (const auto& shader_type : shader_types) {
        if (!shader_stages_.contains(shader_type)) {
          throw std::runtime_error("Missing shader stage");
        }
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

      const std::string pipeline_name = std::string(pipeline_name_) + " - Graphics Pipeline";
      gpu_->set_debug_name(pipeline_name, VK_OBJECT_TYPE_PIPELINE,
                           reinterpret_cast<uint64_t>(pipeline_));

      for (const auto& shader_module : shader_modules_ | std::views::values) {
        vkDestroyShaderModule(gpu_->getDevice(), shader_module, nullptr);
      }
    }

    void create_compute_pipeline() {
      if (!shader_stages_.contains(VK_SHADER_STAGE_COMPUTE_BIT)) {
        throw std::runtime_error("Missing shader stage");
      }

      const VkComputePipelineCreateInfo pipeline_info = {
          .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
          .pNext = nullptr,
          .flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
          .stage = shader_stages_.at(VK_SHADER_STAGE_COMPUTE_BIT),
          .layout = pipeline_layout_,
      };

      VK_CHECK(vkCreateComputePipelines(gpu_->getDevice(), VK_NULL_HANDLE, 1, &pipeline_info,
                                        nullptr, &pipeline_));

      const std::string pipeline_name = std::string(pipeline_name_) + " - Compute Pipeline";
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
        std::span<const ResourceBinding<ImageInstance>> image_bindings,
        std::span<const ResourceBinding<BufferInstance>> buffer_bindings) {
      using BindingMap = std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>;
      std::unordered_map<uint32_t, BindingMap> descriptor_sets;

      // Helper lambda to process bindings
      auto process_bindings = [&descriptor_sets](const auto& bindings) {
        for (const auto& binding : bindings) {
          const auto& info = binding.info;
          VkDescriptorSetLayoutBinding layout_binding{};
          layout_binding.binding = info.binding_index;
          layout_binding.descriptorType = info.descriptor_type;
          layout_binding.descriptorCount = info.descriptor_count;
          layout_binding.stageFlags = info.shader_stages;

          descriptor_sets[info.set_index][info.binding_index] = layout_binding;
        }
      };

      // Process image and buffer bindings
      process_bindings(image_bindings);
      process_bindings(buffer_bindings);

      // Create the descriptor layouts
      create_descriptor_layout(std::move(descriptor_sets));

      // Group bindings by set index
      std::unordered_map<uint32_t, std::vector<ResourceBinding<ImageInstance>>>
          image_bindings_by_set;
      std::unordered_map<uint32_t, std::vector<ResourceBinding<BufferInstance>>>
          buffer_bindings_by_set;

      for (const auto& binding : image_bindings) {
        image_bindings_by_set[binding.info.set_index].push_back(binding);
      }

      for (const auto& binding : buffer_bindings) {
        buffer_bindings_by_set[binding.info.set_index].push_back(binding);
      }

      // Iterate over each set index to create descriptor buffers and write descriptors
      for (const auto& [set_index, bindings] : set_bindings_) {
        std::vector<uint32> binding_indices;
        for (const auto& binding_index : bindings | std::views::keys) {
          binding_indices.push_back(binding_index);
        }

        // Create DescriptorBufferInstance
        const std::string descriptor_buffer_name
            = std::string(pipeline_name_) + " Set " + std::to_string(set_index)
              + " Descriptor Buffer";
        auto descriptor_buffer = std::make_shared<DescriptorBufferInstance>(
            gpu_, descriptor_buffer_name, descriptor_set_layouts_.at(set_index), binding_indices);

        // Process image bindings for this set index
        if (auto it = image_bindings_by_set.find(set_index); it != image_bindings_by_set.end()) {
          for (const auto& binding : it->second) {
            if (binding.info.descriptor_type == VK_DESCRIPTOR_TYPE_MAX_ENUM)
              continue;  // image attachment

            const auto& info = binding.info;
            VkDescriptorImageInfo image_info{};
            image_info.sampler = binding.info.sampler.value_or(VK_NULL_HANDLE);
            image_info.imageView = binding.resource->get_image_view();
            image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;  // TODO: Set appropriate layout

            descriptor_buffer->write_image(info.binding_index, info.descriptor_type,
                                           std::move(image_info));
          }
        }

        // Process buffer bindings for this set index
        if (auto it = buffer_bindings_by_set.find(set_index); it != buffer_bindings_by_set.end()) {
          for (const auto& binding : it->second) {
            const auto& info = binding.info;
            descriptor_buffer->write_buffer(info.binding_index, info.descriptor_type,
                                            binding.resource->get_address(),
                                            binding.resource->get_requested_size());
          }
        }

        descriptor_buffer->update();

        descriptor_buffers_.emplace(set_index, std::move(descriptor_buffer));
      }
    }


    void create_pipeline_layout(const VkPushConstantRange push_constant_range) {
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

      VK_CHECK(vkCreatePipelineLayout(gpu_->getDevice(), &pipeline_layout_create_info, nullptr,
                                      &pipeline_layout_));

      const std::string name = std::string(pipeline_name_) + " Pipeline Layout";
      gpu_->set_debug_name(name, VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                           reinterpret_cast<uint64_t>(pipeline_layout_));
    }

    void bind_descriptors(const CommandBuffer cmd,
                          const VkPipelineBindPoint bind_point) {
      std::vector<VkDescriptorBufferBindingInfoEXT> buffer_bindings;
      buffer_bindings.reserve(descriptor_buffers_.size());

      for (const auto& descriptor_buffer : descriptor_buffers_ | std::views::values) {
        buffer_bindings.push_back({.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
                                  .address = descriptor_buffer->get_address(),
                                  .usage = descriptor_buffer->get_usage()});
      }
      //TODO bind them once at the start of the frame
      vkCmdBindDescriptorBuffersEXT(cmd.get(), buffer_bindings.size(), buffer_bindings.data());

      for (const auto& [set_index, descriptor_buffer] : descriptor_buffers_) {
        descriptor_buffer->bind_descriptors(cmd.get(), bind_point, pipeline_layout_, set_index);
      }
    }
 
    VkPipelineLayout get_pipeline_layout() const { return pipeline_layout_; }
    VkPipeline get_pipeline() const { return pipeline_; }
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

  class GraphicsPipeline {
    PipelineUtil util_;
    VkPipelineBindPoint bind_point_ = VK_PIPELINE_BIND_POINT_GRAPHICS;

  public:
         
    GraphicsPipeline(IGpu* gpu, const std::string_view pipeline_name,
                     const std::span<const ResourceBinding<ImageInstance>> image_bindings,
                     const std::span<const ResourceBinding<BufferInstance>> buffer_bindings,
                     const VkPushConstantRange push_constant_range, std::string&& task_source,
                     std::string&& mesh_source, std::string&& fragment_source,
                     VkGraphicsPipelineCreateInfo&& pipeline_create_info)
        : util_(gpu, pipeline_name) {
      util_.create_descriptor_layout(image_bindings, buffer_bindings);
      util_.add_shader(std::move(task_source), VK_SHADER_STAGE_TASK_BIT_EXT);
      util_.add_shader(std::move(mesh_source), VK_SHADER_STAGE_MESH_BIT_EXT);
      util_.add_shader(std::move(fragment_source), VK_SHADER_STAGE_FRAGMENT_BIT);
      util_.create_pipeline_layout(push_constant_range);
      util_.create_graphics_pipeline(pipeline_create_info,
                                     {VK_SHADER_STAGE_TASK_BIT_EXT, VK_SHADER_STAGE_MESH_BIT_EXT,
                                      VK_SHADER_STAGE_FRAGMENT_BIT});
    }
         
    GraphicsPipeline(IGpu* gpu, const std::string_view pipeline_name,
                     const std::span<const ResourceBinding<ImageInstance>> image_bindings,
                     const std::span<const ResourceBinding<BufferInstance>> buffer_bindings,
                     const VkPushConstantRange push_constant_range, std::string&& vertex_source,
                     std::string&& fragment_source,
                     VkGraphicsPipelineCreateInfo&& pipeline_create_info)
        : util_(gpu, pipeline_name) {
      util_.create_descriptor_layout(image_bindings, buffer_bindings);
      util_.add_shader(std::move(vertex_source), VK_SHADER_STAGE_VERTEX_BIT);
      util_.add_shader(std::move(fragment_source), VK_SHADER_STAGE_FRAGMENT_BIT);
      util_.create_pipeline_layout(push_constant_range);
      util_.create_graphics_pipeline(pipeline_create_info,
                                     {VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT});
    }

    void bind(const CommandBuffer cmd) {
      util_.bind_descriptors(cmd, bind_point_);
      cmd.bind_pipeline(bind_point_, util_.get_pipeline());
    }

    void begin_render_pass(
        const CommandBuffer cmd,
        const std::unordered_map<uint32, std::shared_ptr<ImageInstance>>& color_attachments,
        const std::shared_ptr<ImageInstance>& depth_attachment) {
      std::vector<VkRenderingAttachmentInfo> color_attachment_info;
      VkRenderingAttachmentInfo depth_attachment_info = {};
      VkExtent2D extent = {0, 0};
      VkViewport viewport{
          .x = 0,
          .y = 0,
          .minDepth = 0.f,
          .maxDepth = 1.f,
      };
      VkRect2D scissor{
          .offset = {0, 0},
      };

      if (!color_attachments.empty()) {
        color_attachment_info.reserve(color_attachments.size());
        for (int i = 0; i < color_attachments.size(); ++i) {
          const auto& attachment = color_attachments.at(i);
          color_attachment_info.push_back(vkinit::attachment_info(attachment->get_image_view(),
                                                             nullptr, attachment->get_layout()));
          extent = {attachment->get_extent().width, attachment->get_extent().height};
        }
      }

      if (depth_attachment != nullptr) {
        depth_attachment_info = vkinit::attachment_info(depth_attachment->get_image_view(),
                                                  nullptr, depth_attachment->get_layout());
        extent = {depth_attachment->get_extent().width, depth_attachment->get_extent().height};
      }

      viewport.width = static_cast<float>(extent.width);
      viewport.height = static_cast<float>(extent.height);
      scissor.extent.width = extent.width;
      scissor.extent.height = extent.height;

      VkRenderingInfo rendering_info;
      if (color_attachment_info.empty()) {
        rendering_info = vkinit::rendering_info(extent, nullptr,
                                                depth_attachment_info.sType ? &depth_attachment_info : nullptr);
      } else {
        rendering_info = vkinit::rendering_info_for_gbuffer(
            extent, color_attachment_info.data(), color_attachment_info.size(),
                                     depth_attachment_info.sType ? &depth_attachment_info : nullptr);
      }

      cmd.begin_rendering(rendering_info);
      cmd.set_viewport(0, 1, &viewport);
      cmd.set_scissor(0, 1, &scissor);
    }

    [[nodiscard]] VkPipelineLayout get_pipeline_layout() const { return util_.get_pipeline_layout(); }

    [[nodiscard]] VkPipeline get_pipeline() const { return util_.get_pipeline(); }

    [[nodiscard]] VkPipelineBindPoint get_bind_point() const { return bind_point_; }

  };

  class ComputePipeline {
    PipelineUtil util_;
    VkPipelineBindPoint bind_point_ = VK_PIPELINE_BIND_POINT_COMPUTE;

  public:

    ComputePipeline(IGpu* gpu, const std::string_view pipeline_name,
                    const std::span<const ResourceBinding<ImageInstance>> image_bindings,
                    const std::span<const ResourceBinding<BufferInstance>> buffer_bindings,
                    const VkPushConstantRange push_constant_range,
                    const std::string&& compute_source)
        : util_(gpu, pipeline_name) {
      util_.create_descriptor_layout(image_bindings, buffer_bindings);
      util_.add_shader(std::move(compute_source), VK_SHADER_STAGE_COMPUTE_BIT);
      util_.create_pipeline_layout(push_constant_range);
      util_.create_compute_pipeline();
    }

    void bind(const CommandBuffer cmd) {
      util_.bind_descriptors(cmd, bind_point_);
      cmd.bind_pipeline(bind_point_, util_.get_pipeline());
    }

    [[nodiscard]] VkPipelineLayout get_pipeline_layout() const { return util_.get_pipeline_layout(); }

    [[nodiscard]] VkPipeline get_pipeline() const { return util_.get_pipeline(); }

    [[nodiscard]] VkPipelineBindPoint get_bind_point() const { return bind_point_; }
  };

  struct ResourceComponentBindings {
    std::unordered_map<uint32, std::shared_ptr<ImageInstance>> color_attachments;
    std::shared_ptr<ImageInstance> depth_attachment;

    std::vector<ResourceBinding<ImageInstance>> image_bindings;
    std::vector<ResourceBinding<BufferInstance>> buffer_bindings;

    PushDescriptor push_descriptor;

    ResourceComponentBindings& add_attachment(const std::shared_ptr<ImageInstance>& image,
                                              uint32 attachment_index = 0) {
      if (!image) {
        throw std::invalid_argument("Image cannot be null");
      }

      switch (image->get_type()) {
        case TextureType::kColor:
          color_attachments.emplace(attachment_index, image);
          break;
        case TextureType::kDepth:
          if (attachment_index != 0) {
            throw std::runtime_error("Depth attachment index must be 0");
          }
          depth_attachment = image;
          break;
        default:
          throw std::runtime_error("Unsupported attachment type");
      }

      image_bindings.push_back(
          {image,
           {0, 0, VK_DESCRIPTOR_TYPE_MAX_ENUM, VK_SHADER_STAGE_FRAGMENT_BIT, 1, nullptr},
           ResourceUsage::WRITE});
      return *this;
    }

    // Add an image binding
    ResourceComponentBindings& add_binding(uint32 set_index, uint32 binding_index,
                                           const std::shared_ptr<ImageInstance>& image,
                                           VkSampler sampler, ResourceUsage usage,
                                           VkDescriptorType descriptor_type,
                                           VkShaderStageFlags shader_stages,
                                           uint32 descriptor_count = 1) {
      if (!image) {
        throw std::invalid_argument("Image cannot be null");
      }

      image_bindings.push_back(
          {image,
           {set_index, binding_index, descriptor_type, shader_stages, descriptor_count, sampler},
           usage});
      return *this;
    }

    // Add a buffer binding
    ResourceComponentBindings& add_binding(uint32 set_index, uint32 binding_index,
                                           const std::shared_ptr<BufferInstance>& buffer,
                                           ResourceUsage usage, VkDescriptorType descriptor_type,
                                           VkShaderStageFlags shader_stages) {
      if (!buffer) {
        throw std::invalid_argument("Buffer cannot be null");
      }

      buffer_bindings.push_back(
          {buffer, {set_index, binding_index, descriptor_type, shader_stages, 1}, usage});
      return *this;
    }

    // Add push constants
    ResourceComponentBindings& add_push_constant(uint32_t size, VkShaderStageFlags stage_flags) {
      push_descriptor = PushDescriptor(size, stage_flags);
      return *this;
    }
  };


  class ResourceComponent {
  public:
    explicit ResourceComponent(ResourceComponentBindings&& bindings) : data_(std::move(bindings)) {}

    // Retrieve all resources based on usage
    [[nodiscard]] std::vector<ResourceBinding<ResourceInstance>> get_resources(
        ResourceUsage usage) const {
      std::vector<ResourceBinding<ResourceInstance>> result;
      append_bindings(result, data_.image_bindings, usage);
      append_bindings(result, data_.buffer_bindings, usage);
      return result;
    }

    [[nodiscard]] ResourceBinding<BufferInstance> get_buffer_binding(
        const uint32 set_index, const uint32 binding_index) const {
      for (const auto& resource : data_.buffer_bindings) {
        if (resource.info.set_index == set_index && resource.info.binding_index == binding_index) {
          return resource;
        }
      }
      throw std::runtime_error("Resource binding not found");
    }

    [[nodiscard]] ResourceBinding<ImageInstance> get_image_binding(
        const uint32 set_index, const uint32 binding_index) const {
      for (const auto& resource : data_.image_bindings) {
        if (resource.info.set_index == set_index && resource.info.binding_index == binding_index) {
          return resource;
        }
      }
      throw std::runtime_error("Resource binding not found");
    }

    // Get image or buffer bindings as spans
    [[nodiscard]] std::span<const ResourceBinding<ImageInstance>> get_image_bindings() const {
      return data_.image_bindings;
    }

    [[nodiscard]] std::span<const ResourceBinding<BufferInstance>> get_buffer_bindings() const {
      return data_.buffer_bindings;
    }

    // Retrieve specific attachments
    [[nodiscard]] const auto& get_color_attachments() const { return data_.color_attachments; }
    [[nodiscard]] const auto& get_depth_attachment() const { return data_.depth_attachment; }

    // Get individual bindings
    [[nodiscard]] VkPushConstantRange get_push_constant_range() const {
      return data_.push_descriptor.get_push_constant_range();
    }

  private:
    ResourceComponentBindings data_;

    // Helper function to append bindings with type conversion
    template <typename T>
    void append_bindings(std::vector<ResourceBinding<ResourceInstance>>& result,
                         const std::vector<ResourceBinding<T>>& bindings,
                         ResourceUsage usage) const {
      for (const auto& binding : bindings) {
        if (binding.usage == usage) {
          result.push_back({std::static_pointer_cast<ResourceInstance>(binding.resource),
                            binding.info, binding.usage});
        }
      }
    }
  };

  class RenderPass {
    std::string name_;
  protected:
    explicit RenderPass(std::string name) : name_(std::move(name)) {
      fmt::println("Compiling Render Pass: {}", name_);
    }
  public:
    [[nodiscard]] std::string_view get_name() const { return name_; }
    virtual std::vector<ResourceBinding<ResourceInstance>> get_resources(ResourceUsage usage) = 0;
    virtual VkPipelineBindPoint get_bind_point() = 0;
    virtual void execute(CommandBuffer cmd) = 0;
    virtual ~RenderPass() = default;
  };

  class DrawCullDirectionalDepthPass : public RenderPass {
    // TODO
    struct alignas(16) DrawCullDepthConstants {
      int32 draw_count;
    };
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<int32()> draw_count_provider_;  // function to compute draw count

  public:
    DrawCullDirectionalDepthPass(const std::shared_ptr<BufferInstance>& camera_buffer,
                                 const std::shared_ptr<BufferInstance>& task_commands,
                                 const std::shared_ptr<BufferInstance>& draws,
                                 const std::shared_ptr<BufferInstance>& command_count, IGpu* gpu,
                                 std::function<int32()> draw_count_provider)
        : RenderPass("Draw Cull Directional Depth Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, camera_buffer, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 6, draws, ResourceUsage::READ, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 5, task_commands, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 7, command_count, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_push_constant(sizeof(DrawCullDepthConstants), VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_push_constant_range(),
                            "draw_cull_depth.comp.spv"),
          draw_count_provider_(std::move(draw_count_provider)) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    VkPipelineBindPoint get_bind_point() override { return compute_pipeline_.get_bind_point();}

    void execute(const CommandBuffer cmd) override {
      const auto command_count = resources_.get_buffer_binding(1, 7).resource;

      cmd.fill_buffer(command_count->get_buffer_handle(), 0,
                      command_count->get_size(), 0);

      const int32 max_command_count = draw_count_provider_();
      const uint32 group_count
          = (static_cast<uint32>(max_command_count) + 63) / 64;  // 64 threads per group

      const DrawCullDepthConstants draw_cull_constants{.draw_count = max_command_count};

      compute_pipeline_.bind(cmd);

      cmd.push_constants(compute_pipeline_.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(DrawCullDepthConstants), &draw_cull_constants);
      cmd.dispatch(group_count, 1, 1);
    }
  };

  class TaskSubmitDirectionalDepthPass final : public RenderPass {
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;

  public:
    TaskSubmitDirectionalDepthPass(const std::shared_ptr<BufferInstance>& task_commands,
                                   const std::shared_ptr<BufferInstance>& command_count, IGpu* gpu)
        : RenderPass("Task Submit Directional Depth Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 5,task_commands, ResourceUsage::READ, 
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(0, 5,task_commands, ResourceUsage::WRITE, 
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(0, 7,command_count, ResourceUsage::WRITE, 
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_push_constant_range(),
                            "task_submit.comp.spv") {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    VkPipelineBindPoint get_bind_point() override { return compute_pipeline_.get_bind_point(); }

    void execute(const CommandBuffer cmd) override {
      compute_pipeline_.bind(cmd);
      cmd.dispatch(1, 1, 1);
    }
  };

  class MeshletDirectionalDepthPass final : public RenderPass {
    struct alignas(16) MeshletDepthPushConstants {
      int cullFlags{0};
      float32 pyramidWidth, pyramidHeight;  // depth pyramid size in texels
      int32 clusterOcclusionEnabled;
    };

    ResourceComponent resources_;
    GraphicsPipeline graphics_pipeline_;

  public:
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
                                const std::shared_ptr<BufferInstance>& command_count,
                                const std::shared_ptr<ImageInstance>& shadow_map, IGpu* gpu)
        : RenderPass("Meshlet Directional Depth Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding( 0, 0,camera_buffer, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT
                                   | VK_SHADER_STAGE_FRAGMENT_BIT)
                  .add_binding( 1, 0,light_matrices, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_MESH_BIT_EXT)
                  .add_binding(1, 1,directional_light, ResourceUsage::READ, 
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_MESH_BIT_EXT)
                  .add_binding(1, 2,point_light, ResourceUsage::READ, 
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_MESH_BIT_EXT)
                  .add_binding( 2, 0,vertex_positions, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 1,vertex_data, ResourceUsage::READ, 
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 2,meshlet, ResourceUsage::READ, 
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 3,meshlet_vertices, ResourceUsage::READ, 
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding( 2, 4,meshlet_indices, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 5,task_commands, ResourceUsage::READ, 
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 6,draws, ResourceUsage::READ,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 7, command_count, ResourceUsage::READ,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_push_constant(sizeof(MeshletDepthPushConstants),
                                     VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT)
                  .add_attachment(shadow_map))),

          graphics_pipeline_(
              gpu, get_name(), resources_.get_image_bindings(), resources_.get_buffer_bindings(),
              resources_.get_push_constant_range(), "geometry_depth.task.spv",
              "geometry_depth.mesh.spv", "geometry_depth.frag.spv",
              GraphicsPipelineBuilder()
                  .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                  .set_polygon_mode(VK_POLYGON_MODE_FILL)
                  .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                  .set_multisampling_none()
                  .disable_blending()
                  .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
                  .set_depth_format(resources_.get_depth_attachment()->get_format())
                  .build_pipeline_info()) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    VkPipelineBindPoint get_bind_point() override { return graphics_pipeline_.get_bind_point(); }

    void execute(const CommandBuffer cmd) override {
      graphics_pipeline_.begin_render_pass(cmd, resources_.get_color_attachments(),
                                           resources_.get_depth_attachment());
      graphics_pipeline_.bind(cmd);

      constexpr MeshletDepthPushConstants draw_cull_constants{.cullFlags = 1};
      cmd.push_constants(graphics_pipeline_.get_pipeline_layout(),
                         VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT, 0,
                         sizeof(MeshletDepthPushConstants), &draw_cull_constants);
      const auto command_count = resources_.get_buffer_binding(2, 7).resource;
      cmd.draw_mesh_tasks_indirect_ext(command_count->get_buffer_handle(), sizeof(uint32), 1, 0);
      cmd.end_rendering();
    }
  };

  
  class DrawCullPass final : public RenderPass {
    struct alignas(16) DrawCullConstants {
      int32 draw_count;
    };
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<int32()> draw_count_provider_;  // Function to compute draw count

  public:
    DrawCullPass(const std::shared_ptr<BufferInstance>& camera_buffer,
                                 const std::shared_ptr<BufferInstance>& task_commands,
                                 const std::shared_ptr<BufferInstance>& draws,
                                 const std::shared_ptr<BufferInstance>& command_count, IGpu* gpu,
                                 std::function<int32()> draw_count_provider)
        : RenderPass("Draw Cull Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, camera_buffer, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 6, draws, ResourceUsage::READ, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 5, task_commands, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 7, command_count, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_push_constant(sizeof(DrawCullConstants), VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_push_constant_range(),
                            "draw_cull.comp.spv"),
          draw_count_provider_(std::move(draw_count_provider)) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    VkPipelineBindPoint get_bind_point() override { return compute_pipeline_.get_bind_point(); }

    void execute(const CommandBuffer cmd) override {
      const auto command_count = resources_.get_buffer_binding(1, 7).resource;

      cmd.fill_buffer(command_count->get_buffer_handle(), 0, command_count->get_size(), 0);

      const int32 max_command_count = draw_count_provider_();
      const uint32 group_count
          = (static_cast<uint32>(max_command_count) + 63) / 64;  // 64 threads per group

      const DrawCullConstants draw_cull_constants{.draw_count = max_command_count};

      compute_pipeline_.bind(cmd);

      cmd.push_constants(compute_pipeline_.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(DrawCullConstants), &draw_cull_constants);
      cmd.dispatch(group_count, 1, 1);
    }
  };

  class TaskSubmitPass final : public RenderPass {
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;

  public:
    TaskSubmitPass(const std::shared_ptr<BufferInstance>& task_commands,
                                   const std::shared_ptr<BufferInstance>& command_count, IGpu* gpu)
        : RenderPass("Task Submit Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 5, task_commands, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(0, 5, task_commands, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(0, 7, command_count, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_push_constant_range(),
                            "task_submit.comp.spv") {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    VkPipelineBindPoint get_bind_point() override { return compute_pipeline_.get_bind_point(); }

    void execute(const CommandBuffer cmd) override {
      compute_pipeline_.bind(cmd);
      cmd.dispatch(1, 1, 1);
    }
  };

  class MeshletPass final : public RenderPass {
    ResourceComponent resources_;
    GraphicsPipeline graphics_pipeline_;

    struct alignas(16) MeshletPushConstants {
      int cullFlags{0};
      float32 pyramidWidth, pyramidHeight;  // depth pyramid size in texels
      int32 clusterOcclusionEnabled;
    };

  public:
    MeshletPass(const std::shared_ptr<BufferInstance>& camera_buffer,
                const std::shared_ptr<BufferInstance>& materials,
                const std::shared_ptr<BufferInstance>& vertex_positions,
                const std::shared_ptr<BufferInstance>& vertex_data,
                const std::shared_ptr<BufferInstance>& meshlet,
                const std::shared_ptr<BufferInstance>& meshlet_vertices,
                const std::shared_ptr<BufferInstance>& meshlet_indices,
                const std::shared_ptr<BufferInstance>& task_commands,
                const std::shared_ptr<BufferInstance>& draws,
                const std::shared_ptr<BufferInstance>& command_count,
                const std::shared_ptr<ImageInstance>& g_buffer_1,
                const std::shared_ptr<ImageInstance>& g_buffer_2,
                const std::shared_ptr<ImageInstance>& g_buffer_3,
                const std::shared_ptr<ImageInstance>& g_buffer_depth, IGpu* gpu)
        : RenderPass("Meshlet Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, camera_buffer, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT
                                   | VK_SHADER_STAGE_FRAGMENT_BIT)
                  //.add_binding(1, 3, textures, ResourceUsage::READ,
                  //           VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                  .add_binding(1, 4, materials, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                  .add_binding(2, 0, vertex_positions, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 1, vertex_data, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 2, meshlet, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 3, meshlet_vertices, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 4, meshlet_indices, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 5, task_commands, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 6, draws, ResourceUsage::READ, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 7, command_count, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_push_constant(sizeof(MeshletPushConstants),
                                     VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT)
                  .add_attachment(g_buffer_1, 0)
                  .add_attachment(g_buffer_2, 1)
                  .add_attachment(g_buffer_3, 2)
                  .add_attachment(g_buffer_depth))),
          graphics_pipeline_(
              gpu, get_name(), resources_.get_image_bindings(), resources_.get_buffer_bindings(),
              resources_.get_push_constant_range(), "geometry.task.spv", "geometry.mesh.spv",
              "geometry_deferred.frag.spv",
              GraphicsPipelineBuilder()
                  .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                  .set_polygon_mode(VK_POLYGON_MODE_FILL)
                  .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                  .set_multisampling_none()
                  .disable_blending(3)
                  .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
                  .set_depth_format(resources_.get_depth_attachment()->get_format())
                  .build_pipeline_info()) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    VkPipelineBindPoint get_bind_point() override { return graphics_pipeline_.get_bind_point(); }

    void execute(const CommandBuffer cmd) override {
      graphics_pipeline_.begin_render_pass(cmd, resources_.get_color_attachments(),
                                           resources_.get_depth_attachment());
      graphics_pipeline_.bind(cmd);

      constexpr MeshletPushConstants draw_cull_constants{};
      cmd.push_constants(graphics_pipeline_.get_pipeline_layout(),
                         VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT, 0,
                         sizeof(MeshletPushConstants), &draw_cull_constants);
      const auto command_count = resources_.get_buffer_binding(2, 7).resource;
      cmd.draw_mesh_tasks_indirect_ext(command_count->get_buffer_handle(), sizeof(uint32), 1, 0);
      cmd.end_rendering();
    }
  };

  class SsaoPass final : public RenderPass {
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<RenderConfig::SsaoParams()>
        push_constant_provider_; 
  public:
    SsaoPass(const std::shared_ptr<BufferInstance>& camera_buffer,
             const std::shared_ptr<ImageInstance>& g_buffer_depth,
             const std::shared_ptr<ImageInstance>& g_buffer_2,
             const std::shared_ptr<ImageInstance>& rotation_texture,
             const std::shared_ptr<ImageInstance>& ambient_occlusion,
             const VkSampler post_process_sampler, IGpu* gpu,
             const std::function<RenderConfig::SsaoParams()>& push_constant_provider)
        : RenderPass("Ssao Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, camera_buffer, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 0, g_buffer_depth, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 1, g_buffer_2, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 2, rotation_texture, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 3, ambient_occlusion, nullptr, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_push_constant(sizeof(RenderConfig::SsaoParams),
                                     VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_push_constant_range(),
                            "ssao_filter.comp.spv"),
          push_constant_provider_(push_constant_provider) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    VkPipelineBindPoint get_bind_point() override { return compute_pipeline_.get_bind_point(); }

    void execute(const CommandBuffer cmd) override {
      compute_pipeline_.bind(cmd);

      const auto push_constants = push_constant_provider_();
      cmd.push_constants(compute_pipeline_.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(RenderConfig::SsaoParams), &push_constants);
      const auto [width, height, _] = resources_.get_image_binding(1, 3).resource->get_extent();
      //cmd.dispatch(width / 8, height / 8, 1);
    }
  };

  class VolumetricLightingInjectionPass final : public RenderPass {
    struct alignas(16) VolumetricLightingInjectionPassConstants {
      glm::vec2 halton_xy{0.f, 0.f};  // no jitter
      float32 temporal_reprojection_jitter_scale{0.f};
      float32 density_modifier{1.f};

      glm::uvec3 froxel_dimensions{128, 128, 128};
      int32 current_frame{0};

      float32 noise_scale{1.f};
      uint32 noise_type{0};
      float32 froxel_near{1.f};
      float32 froxel_far{100.f};

      glm::mat4 froxel_inverse_view_projection{1.f};

      float32 volumetric_noise_position_multiplier{1.f};
      float32 volumetric_noise_speed_multiplier{1.f};
      float32 height_fog_density{0.01f};
      float32 height_fog_falloff{1.f};

      glm::vec3 box_position{0.f};
      float32 box_fog_density{0.1f};

      glm::vec3 box_size{10.f};
      float32 scattering_factor{0.5f};
    };
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<RenderConfig::VolumetricLightingParams()> push_constant_provider_;
    std::function<uint32()> frame_provider_;
    std::function<PerFrameData()> camera_provider_;

  public:
    VolumetricLightingInjectionPass(
        const std::shared_ptr<ImageInstance>& blue_noise,
        const std::shared_ptr<ImageInstance>& noise_texture,
        const std::shared_ptr<ImageInstance>& froxel_data, const VkSampler post_process_sampler,
        const std::function<RenderConfig::VolumetricLightingParams()>& push_constant_provider,
        const std::function<uint32()>& frame_provider,
        const std::function<PerFrameData()>& camera_provider, IGpu* gpu)
        : RenderPass("Volumetric Lighting Injection Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, blue_noise, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(0, 1, noise_texture, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(0, 2, froxel_data, nullptr, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_push_constant(sizeof(VolumetricLightingInjectionPassConstants),
                                     VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_push_constant_range(),
                            "volumetric_light_injection.comp.spv"),
          push_constant_provider_(push_constant_provider),
          frame_provider_(frame_provider),
          camera_provider_(camera_provider) {}

   std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    VkPipelineBindPoint get_bind_point() override { return compute_pipeline_.get_bind_point(); }

    void execute(const CommandBuffer cmd) override {
      compute_pipeline_.bind(cmd);

      const auto params = push_constant_provider_();
      const auto perFrameData = camera_provider_();
      VolumetricLightingInjectionPassConstants push_constants;
      push_constants.current_frame = frame_provider_();
      push_constants.froxel_inverse_view_projection = perFrameData.inv_viewProj;
      push_constants.density_modifier = params.density_modifier;
      push_constants.noise_scale = params.noise_scale;
      push_constants.noise_type = params.noise_type;
      push_constants.volumetric_noise_position_multiplier
          = params.volumetric_noise_position_multiplier;
      push_constants.froxel_near = perFrameData.znear;
      push_constants.froxel_far = perFrameData.zfar;
      push_constants.height_fog_density = params.height_fog_density;
      push_constants.height_fog_falloff = params.height_fog_falloff;
      push_constants.box_position = params.box_position;
      push_constants.box_fog_density = params.box_fog_density;
      push_constants.box_size = params.box_size;
      push_constants.scattering_factor = params.scattering_factor;

      cmd.push_constants(compute_pipeline_.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(VolumetricLightingInjectionPassConstants), &push_constants);
      const auto [width, height, _] = resources_.get_image_binding(0, 2).resource->get_extent();
      //cmd.dispatch((uint32)ceil(width / 8), (uint32)ceil(height / 8), 128);
    }
  };

  class VolumetricLightingScatteringPass final : public RenderPass {
    struct alignas(16) VolumetricLightingScatteringPassConstants {
      glm::vec2 halton_xy{0.f, 0.f};  // no jitter
      float32 temporal_reprojection_jitter_scale{0.f};
      float32 density_modifier{1.f};

      glm::uvec3 froxel_dimensions{128, 128, 128};
      int32 current_frame{0};

      float32 noise_scale{1.f};
      uint32 noise_type{0};
      float32 froxel_near{1.f};
      float32 froxel_far{100.f};

      glm::mat4 froxel_inverse_view_projection{1.f};

      float32 scattering_factor{0.5f};
      float32 phase_anisotropy{0.2f};
      uint32 phase_type{0};
      uint32 num_point_lights;
    };
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<RenderConfig::VolumetricLightingParams()> push_constant_provider_;
    std::function<uint32()> frame_provider_;
    std::function<PerFrameData()> camera_provider_;
    std::function<uint32()> point_light_count_provider_;

  public:
    VolumetricLightingScatteringPass(
        const std::shared_ptr<BufferInstance>& camera_buffer,
        const std::shared_ptr<BufferInstance>& light_matrices,
        const std::shared_ptr<BufferInstance>& directional_lights,
        const std::shared_ptr<BufferInstance>& points_lights,
        const std::shared_ptr<ImageInstance>& blue_noise,
        const std::shared_ptr<ImageInstance>& froxel_data,
        const std::shared_ptr<ImageInstance>& shadow_map,
        const std::shared_ptr<ImageInstance>& light_scattering,
        const VkSampler post_process_sampler,
        const std::function<RenderConfig::VolumetricLightingParams()>& push_constant_provider,
        const std::function<uint32()>& frame_provider,
        const std::function<PerFrameData()>& camera_provider,
        const std::function<uint32()>& point_light_count_provider, IGpu* gpu)
        : RenderPass("Volumetric Lighting Scattering Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, camera_buffer, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)

                  .add_binding(1, 0, blue_noise, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 1, froxel_data, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 2, shadow_map, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 3, light_scattering, nullptr, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               VK_SHADER_STAGE_COMPUTE_BIT)

                  .add_binding(2, 0, light_matrices, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(2, 1, directional_lights, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(2, 2, points_lights, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_push_constant(sizeof(VolumetricLightingScatteringPassConstants),
                                     VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_push_constant_range(),
                            "volumetric_light_scattering.comp.spv"),
          push_constant_provider_(push_constant_provider),
          frame_provider_(frame_provider),
          camera_provider_(camera_provider),
          point_light_count_provider_(point_light_count_provider) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    VkPipelineBindPoint get_bind_point() override { return compute_pipeline_.get_bind_point(); }

    void execute(const CommandBuffer cmd) override {
      compute_pipeline_.bind(cmd);

      const auto params = push_constant_provider_();
      const auto perFrameData = camera_provider_();
      VolumetricLightingScatteringPassConstants push_constants;
      push_constants.current_frame = frame_provider_();
      push_constants.froxel_inverse_view_projection = perFrameData.inv_viewProj;
      push_constants.froxel_near = perFrameData.znear;
      push_constants.froxel_far = perFrameData.zfar;
      push_constants.scattering_factor = params.scattering_factor;
      push_constants.phase_anisotropy = params.phase_anisotropy;
      push_constants.phase_type = params.phase_type;
      push_constants.density_modifier = params.density_modifier;
      push_constants.noise_scale = params.noise_scale;
      push_constants.noise_type = params.noise_type;
      push_constants.num_point_lights = point_light_count_provider_();


      cmd.push_constants(compute_pipeline_.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(VolumetricLightingScatteringPassConstants), &push_constants);
      const auto [width, height, _] = resources_.get_image_binding(1, 3).resource->get_extent();
      //cmd.dispatch((uint32)ceil(width / 8), (uint32)ceil(height / 8), 128);
    }
  };

  
  class VolumetricLightingSpatialFilterPass final : public RenderPass {
    struct VolumetricLightingSpatialFilterPassConstants {
      glm::uvec3 froxel_dimensions{128, 128, 128};
      int32 use_spatial_filtering{1};
    };
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<RenderConfig::VolumetricLightingParams()> push_constant_provider_;

  public:
    VolumetricLightingSpatialFilterPass(
        const std::shared_ptr<ImageInstance>& light_scattering,
        const std::shared_ptr<ImageInstance>& froxel_data, const VkSampler post_process_sampler,
        const std::function<RenderConfig::VolumetricLightingParams()>& push_constant_provider,
        IGpu* gpu)
        : RenderPass("Volumetric Lighting Spatial Filter Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, light_scattering, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(0, 1, froxel_data, nullptr, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_push_constant(sizeof(VolumetricLightingSpatialFilterPassConstants),
                                     VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_push_constant_range(),
                            "volumetric_light_spatial_filter.comp.spv"),
          push_constant_provider_(push_constant_provider) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    VkPipelineBindPoint get_bind_point() override { return compute_pipeline_.get_bind_point(); }

    void execute(const CommandBuffer cmd) override {
      compute_pipeline_.bind(cmd);

      const auto params = push_constant_provider_();
      VolumetricLightingSpatialFilterPassConstants push_constants;
      push_constants.use_spatial_filtering = params.enable_spatial_filter;

      cmd.push_constants(compute_pipeline_.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(VolumetricLightingSpatialFilterPassConstants), &push_constants);
      const auto [width, height, _] = resources_.get_image_binding(0, 1).resource->get_extent();
      //cmd.dispatch((uint32)ceil(width / 8), (uint32)ceil(height / 8), 128);
    }
  };

  class VolumetricLightingIntegrationPass final : public RenderPass {
    struct alignas(16) VolumetricLightingIntegrationPassConstants {
      glm::uvec3 froxel_dimensions{128, 128, 128};
      int32 current_frame{0};

      float32 noise_scale{1.f};
      uint32 noise_type{0};
      float32 froxel_near{1.f};
      float32 froxel_far{100.f};
    };
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<RenderConfig::VolumetricLightingParams()> push_constant_provider_;
    std::function<uint32()> frame_provider_;
    std::function<PerFrameData()> camera_provider_;

  public:
    VolumetricLightingIntegrationPass(
        const std::shared_ptr<ImageInstance>& froxel_data,
        const std::shared_ptr<ImageInstance>& light_integrated,
        const VkSampler post_process_sampler,
        const std::function<RenderConfig::VolumetricLightingParams()>& push_constant_provider,
        const std::function<uint32()>& frame_provider,
        const std::function<PerFrameData()>& camera_provider, IGpu* gpu)
        : RenderPass("Volumetric Lighting Integration Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, froxel_data, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(0, 1, light_integrated, nullptr, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_push_constant(sizeof(VolumetricLightingIntegrationPassConstants),
                                     VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_push_constant_range(),
                            "volumetric_light_spatial_filter.comp.spv"),
          push_constant_provider_(push_constant_provider),
          frame_provider_(frame_provider),
          camera_provider_(camera_provider)
    {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    VkPipelineBindPoint get_bind_point() override { return compute_pipeline_.get_bind_point(); }

    void execute(const CommandBuffer cmd) override {
      compute_pipeline_.bind(cmd);

      const auto params = push_constant_provider_();
      const auto perFrameData = camera_provider_();
      VolumetricLightingIntegrationPassConstants push_constants;
      push_constants.current_frame = frame_provider_();
      push_constants.froxel_near = perFrameData.znear;
      push_constants.froxel_far = perFrameData.zfar;
      push_constants.noise_scale = params.noise_scale;
      push_constants.noise_type = params.noise_type;


      cmd.push_constants(compute_pipeline_.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(VolumetricLightingIntegrationPassConstants), &push_constants);
      const auto [width, height, _] = resources_.get_image_binding(0, 1).resource->get_extent();
      //cmd.dispatch((uint32)ceil(width / 8), (uint32)ceil(height / 8), 128);
    }
  };

  
  class VolumetricLightingNoisePass final : public RenderPass {
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<RenderConfig::VolumetricLightingParams()> push_constant_provider_;
    std::function<uint32()> frame_provider_;
    std::function<PerFrameData()> camera_provider_;

  public:
    VolumetricLightingNoisePass(const std::shared_ptr<ImageInstance>& volumetric_noise_texture,
                                IGpu* gpu)
        : RenderPass("Volumetric Lighting Noise Pass"),
          resources_(std::move(
              ResourceComponentBindings().add_binding(
              0, 0, volumetric_noise_texture, nullptr, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
)),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_push_constant_range(),
                            "volumetric_light_noise.comp.spv") {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    VkPipelineBindPoint get_bind_point() override { return compute_pipeline_.get_bind_point(); }

    void execute(const CommandBuffer cmd) override {
      compute_pipeline_.bind(cmd);
const auto [width, height, _] = resources_.get_image_binding(0, 0).resource->get_extent();
      //cmd.dispatch((uint32)ceil(width / 8), (uint32)ceil(height / 8), 128);
    }
  };

  class LightingPass final : public RenderPass {
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;

  public:
    LightingPass(const std::shared_ptr<BufferInstance>& camera_buffer,
                 const std::shared_ptr<BufferInstance>& material_buffer,
                 const std::shared_ptr<BufferInstance>& light_matrices,
                 const std::shared_ptr<BufferInstance>& directional_light,
                 const std::shared_ptr<BufferInstance>& point_light,
                 const std::shared_ptr<ImageInstance>& g_buffer_1,
                 const std::shared_ptr<ImageInstance>& g_buffer_2,
                 const std::shared_ptr<ImageInstance>& g_buffer_3,
                 const std::shared_ptr<ImageInstance>& g_buffer_depth,
                 const std::shared_ptr<ImageInstance>& shadow_map,
                 const std::shared_ptr<ImageInstance>& integrated_light_scattering,
                 const std::shared_ptr<ImageInstance>& ambient_occlusion,
                 const std::shared_ptr<ImageInstance>& scene_lit,
                 const VkSampler post_process_sampler, IGpu* gpu)
        : RenderPass("Lighting Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, camera_buffer, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)

                  // TODO add textures
                  .add_binding(1, 4, material_buffer, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)

                  .add_binding(2, 0, light_matrices, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(2, 1, directional_light, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(2, 2, point_light, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)

                  .add_binding(3, 0, g_buffer_1, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(3, 1, g_buffer_2, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(3, 2, g_buffer_3, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(3, 3, g_buffer_depth, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(3, 4, shadow_map, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(3, 5, integrated_light_scattering, post_process_sampler,
                               ResourceUsage::READ, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(3, 6, ambient_occlusion, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(3, 7, scene_lit, nullptr, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_push_constant_range(),
                            "pbr_lighting.comp.spv") {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    VkPipelineBindPoint get_bind_point() override { return compute_pipeline_.get_bind_point(); }

    void execute(CommandBuffer cmd) override {
      // draw
    }
  };

  class ToneMapPass final : public RenderPass {
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;

  public:
    ToneMapPass(const std::shared_ptr<ImageInstance>& scene_final,
                const std::shared_ptr<ImageInstance>& scene_lit,
                const VkSampler post_process_sampler, IGpu* gpu)
        : RenderPass("Tone Map Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding( 0, 0,scene_lit, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(0, 0, scene_final, nullptr, ResourceUsage::WRITE, 
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_push_constant_range(),
                            "task_submit.comp.spv") {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    VkPipelineBindPoint get_bind_point() override { return compute_pipeline_.get_bind_point(); }

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

  class SynchronizationManager {
    class SynchronizationVisitor final : public ResourceVisitor {
      CommandBuffer cmd_;
      std::vector<VkImageMemoryBarrier2> image_barriers;
      std::vector<VkBufferMemoryBarrier2> buffer_barriers;

    public:
      explicit SynchronizationVisitor(const CommandBuffer cmd) : cmd_(cmd) {}

      void visit(BufferInstance& buffer, const ResourceUsage usage,
                 const VkShaderStageFlags shader_stage) override {
        // Prepare a buffer memory barrier
        VkBufferMemoryBarrier2 barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
        barrier.buffer = buffer.get_buffer_handle();
        barrier.offset = 0;
        barrier.size = VK_WHOLE_SIZE;

        // Source: from the buffer's current known state
        barrier.srcAccessMask = buffer.get_current_access();
        barrier.srcStageMask = buffer.get_current_stage();

        // Determine destination stages and access masks based on usage
        // If we know the exact shader stage, we could pick a more granular pipeline stage:
        VkPipelineStageFlags2 dstStageMask = 0;
        VkAccessFlags2 dstAccessMask = 0;

        // Assign destination pipeline stages from shader_stage
        // This can be refined if you know whether it's fragment-only, vertex-only, etc.
        if (shader_stage == VK_SHADER_STAGE_ALL_GRAPHICS) {
          dstStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
        } else if (shader_stage == VK_SHADER_STAGE_COMPUTE_BIT) {
          dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        } else {
          // As a fallback for other shader stages:
          dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        }

        // Assign access mask based on usage
        // Refine this depending on whether the buffer is used as a uniform, storage buffer, etc.
        // For general READ: assume uniform buffer or sampled read.
        // For WRITE: assume storage buffer writes.
        if (usage == ResourceUsage::WRITE) {
          // Writing to a storage buffer
          dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
        } else if (usage == ResourceUsage::READ) {
          // Reading from a uniform or storage buffer
          // If uniform: VK_ACCESS_2_UNIFORM_READ_BIT
          // If storage: VK_ACCESS_2_SHADER_STORAGE_READ_BIT
          // For a generic read assumption:
          dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
        }

        // Update the barrier
        barrier.dstAccessMask = dstAccessMask;
        barrier.dstStageMask = dstStageMask;

        // Update the resource state
        buffer.set_current_access(dstAccessMask);
        buffer.set_current_stage(dstStageMask);

        buffer_barriers.push_back(barrier);
      }

      void visit(ImageInstance& image, ResourceUsage usage,
                 VkShaderStageFlags shader_stage) override {
        VkImageMemoryBarrier2 barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.image = image.get_image_handle();
        barrier.subresourceRange = vkinit::image_subresource_range(image.get_image_aspect());

        // Source state
        barrier.oldLayout = image.get_layout();
        barrier.srcAccessMask = image.get_current_access();
        barrier.srcStageMask = image.get_current_stage();

        // Determine pipeline stages
        VkPipelineStageFlags2 dstStageMask = 0;
        if (shader_stage == VK_SHADER_STAGE_ALL_GRAPHICS) {
          dstStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
        } else if (shader_stage == VK_SHADER_STAGE_COMPUTE_BIT) {
          dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        } else {
          dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        }

        // Determine new layout and access based on usage and texture type
        VkImageLayout newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkAccessFlags2 dstAccessMask = 0;
        switch (image.get_type()) {
          case TextureType::kColor:
            if (shader_stage == VK_SHADER_STAGE_ALL_GRAPHICS) {
              if (usage == ResourceUsage::READ) {
                // Sampled read
                newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
              } else {
                // Color attachment write
                newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
              }
            } else {
              // Compute stage
              if (usage == ResourceUsage::READ) {
                newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
              } else {
                // General layout for compute read/write (e.g. storage image)
                newLayout = VK_IMAGE_LAYOUT_GENERAL;
                dstAccessMask
                    = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
              }
            }
            break;

          case TextureType::kDepth:
            if (shader_stage == VK_SHADER_STAGE_ALL_GRAPHICS) {
              if (usage == ResourceUsage::READ) {
                newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
              } else {
                newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
                                | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
              }
            } else {
              // For compute reading a depth texture, typically sampled read:
              if (usage == ResourceUsage::READ) {
                newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
              } else {
                // General for compute write to depth? Usually unusual, but handle generically:
                newLayout = VK_IMAGE_LAYOUT_GENERAL;
                dstAccessMask
                    = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
              }
            }
            break;

          default:
            throw std::runtime_error("Unsupported TextureType for image!");
        }

        barrier.newLayout = newLayout;
        barrier.dstAccessMask = dstAccessMask;
        barrier.dstStageMask = dstStageMask;

        // Update image state
        image.set_layout(newLayout);
        image.set_current_access(dstAccessMask);
        image.set_current_stage(dstStageMask);

        image_barriers.push_back(barrier);
      }


      void apply() const {
        VkDependencyInfo dependency_info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dependency_info.dependencyFlags = 0;
        dependency_info.bufferMemoryBarrierCount = static_cast<uint32_t>(buffer_barriers.size());
        dependency_info.pBufferMemoryBarriers = buffer_barriers.data();
        dependency_info.imageMemoryBarrierCount = static_cast<uint32_t>(image_barriers.size());
        dependency_info.pImageMemoryBarriers = image_barriers.data();
        dependency_info.memoryBarrierCount = 0;
        dependency_info.pMemoryBarriers = nullptr;

        vkCmdPipelineBarrier2(cmd_.get(), &dependency_info);
      }
    };


  public:
    void synchronize_resources(const std::shared_ptr<FrameGraphNode>& node,
                               const CommandBuffer cmd) {
      const auto read_resources = node->render_pass->get_resources(ResourceUsage::READ);
      const auto write_resources = node->render_pass->get_resources(ResourceUsage::WRITE);
      const auto bind_point = node->render_pass->get_bind_point();

      SynchronizationVisitor visitor(cmd);

      for (const auto& [resource, info, _] : read_resources) {
        resource->accept(visitor, ResourceUsage::READ,
                                       info.shader_stages);
      }

      for (const auto& [resource, info, _] : write_resources) {
        resource->accept(visitor, ResourceUsage::WRITE, info.shader_stages);
      }

      visitor.apply();
    }
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
                                                    = CreationType::INTERNAL) {
      auto resource = resource_registry_->add_template(std::move(image_template));
      const uint64 handle = resource->resource_handle;
      if (handle == -1) {
        throw std::runtime_error("Invalid resource handle!");
      }

      auto inserted = edges_.emplace(
          handle, std::make_shared<FrameGraphEdge>(std::move(resource), creation_type));
      if (!inserted.second) {
        throw std::runtime_error("Failed to insert edge into edges map!");
      }

      return std::static_pointer_cast<ImageInstance>(inserted.first->second->resource);
    }

    std::shared_ptr<BufferInstance> add_resource(BufferTemplate&& buffer_template,
                                                 CreationType creation_type
                                                 = CreationType::INTERNAL) {
      auto resource = resource_registry_->add_template(std::move(buffer_template));
      const uint64 handle = resource->resource_handle;
      if (handle == -1) {
        throw std::runtime_error("Invalid resource handle!");
      }

      auto inserted = edges_.emplace(
          handle, std::make_shared<FrameGraphEdge>(std::move(resource), creation_type));
      if (!inserted.second) {
        throw std::runtime_error("Failed to insert edge into edges map!");
      }

      return std::static_pointer_cast<BufferInstance>(inserted.first->second->resource);
    }


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

      uint64 handle = resource->resource_handle;
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