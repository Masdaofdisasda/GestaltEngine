#pragma once 
#include <VulkanTypes.hpp>
#include <algorithm>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <unordered_map>

#include "CommandBuffer.hpp"
#include "VulkanCheck.hpp"
#include "common.hpp"
#include "vk_initializers.hpp"
#include "Interface/IGpu.hpp"
#include "Material/Material.hpp"
#include "Resources/ResourceTypes.hpp"

namespace gestalt::graphics {
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

  class PipelineTool : Moveable<PipelineTool> {
    IGpu* gpu_ = nullptr;
    std::string_view pipeline_name_;

    std::map<uint32, std::map<uint32, VkDescriptorSetLayoutBinding>>
        set_bindings_;

    std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> descriptor_buffers_;
    VkPipelineLayout pipeline_layout_ = nullptr;
    VkPipeline pipeline_ = nullptr;
    std::map<uint32, VkDescriptorSetLayout> descriptor_set_layouts_;
    std::unordered_map<VkShaderStageFlagBits, VkPipelineShaderStageCreateInfo> shader_stages_;
    std::unordered_map<VkShaderStageFlagBits, VkShaderModule> shader_modules_;

    uint32 image_array_set = 0;
    std::optional<ResourceBinding<ImageArrayInstance>> image_array_binding_;

    void create_descriptor_layout(
        std::map<uint32, std::map<uint32, VkDescriptorSetLayoutBinding>>&&
        sets);

  public:
    explicit PipelineTool(IGpu* gpu, const std::string_view pipeline_name)
      : gpu_(gpu),
        pipeline_name_(pipeline_name) {
    }

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

    [[nodiscard]] VkShaderModule load_shader_module(const std::filesystem::path& file_path,
                                                    const VkDevice device) {
      // 1. Open the file in binary mode, cursor at the end
      std::ifstream file(file_path, std::ios::ate | std::ios::binary);
      if (!file.is_open()) {
        throw std::runtime_error(
            fmt::format("Error: Could not open shader file: {}", file_path.string()));
      }

      // 2. Get the file size in bytes
      const auto fileSize = file.tellg();
      if (fileSize < 0) {
        throw std::runtime_error(
            fmt::format("Error: Failed to read file size: {}", file_path.string()));
      }

      // 3. Create a buffer big enough for the entire file
      std::vector<std::uint32_t> buffer(fileSize / sizeof(std::uint32_t));

      // 4. Reset cursor to beginning and read the file into the buffer
      file.seekg(0, std::ios::beg);
      file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
      file.close();

      // 5. Set up the shader module create info
      VkShaderModuleCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
      createInfo.pNext = nullptr;
      createInfo.codeSize = buffer.size() * sizeof(std::uint32_t);
      createInfo.pCode = buffer.data();

      // 6. Create the shader module
      VkShaderModule shaderModule{};
      if (const VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
          result != VK_SUCCESS) {
        throw std::runtime_error(
            fmt::format("Error: Failed to create shader module for file: {}", file_path.string()));
      }

      return shaderModule;
    }

    void add_shader(const std::string&& source_file, VkShaderStageFlagBits shader_stage) {
      const auto shader_path = std::filesystem::current_path() / "../shaders/" / source_file;

      VkShaderModule shader_module = load_shader_module(shader_path, gpu_->getDevice());
      shader_modules_.emplace(shader_stage, shader_module);

      gpu_->set_debug_name(source_file, VK_OBJECT_TYPE_SHADER_MODULE,
                           reinterpret_cast<uint64_t>(shader_module));

      vkinit::pipeline_shader_stage_create_info(shader_stage, shader_module);
      shader_stages_.emplace(
          shader_stage, vkinit::pipeline_shader_stage_create_info(shader_stage, shader_module));
    }


    void create_descriptor_layout(
        std::span<const ResourceBinding<ImageInstance>> image_bindings,
        std::span<const ResourceBinding<BufferInstance>> buffer_bindings,
        std::span<const ResourceBinding<ImageArrayInstance>> image_array_bindings,
        std::span<const ResourceBinding<AccelerationStructureInstance>> tlas_bindings = {}
        ) {
      using BindingMap = std::map<uint32, VkDescriptorSetLayoutBinding>;
      std::map<uint32, BindingMap> descriptor_sets;

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
      process_bindings(image_array_bindings);
      process_bindings(tlas_bindings);

      // Create the descriptor layouts
      create_descriptor_layout(std::move(descriptor_sets));

      // Group bindings by set index
      std::unordered_map<uint32_t, std::vector<ResourceBinding<ImageInstance>>>
          image_bindings_by_set;
      std::unordered_map<uint32_t, std::vector<ResourceBinding<BufferInstance>>>
          buffer_bindings_by_set;
      std::unordered_map<uint32_t, std::vector<ResourceBinding<ImageArrayInstance>>>
          image_array_bindings_by_set;
      std::unordered_map<uint32_t, std::vector<ResourceBinding<AccelerationStructureInstance>>>
          tlas_bindings_by_set;

      for (const auto& binding : image_bindings) {
        image_bindings_by_set[binding.info.set_index].push_back(binding);
      }

      for (const auto& binding : buffer_bindings) {
        buffer_bindings_by_set[binding.info.set_index].push_back(binding);
      }

      for (const auto& binding : image_array_bindings) {
        image_array_bindings_by_set[binding.info.set_index].push_back(binding);
      }

      for (const auto& binding : tlas_bindings) {
        tlas_bindings_by_set[binding.info.set_index].push_back(binding);
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

        if (auto it = image_array_bindings_by_set.find(set_index);
            it != image_array_bindings_by_set.end()) {
          for (const auto& binding : it->second) {
            std::vector<VkDescriptorImageInfo> image_infos;
              const auto& info = binding.info;
            for (const auto& material : binding.resource->get_materials()) {
              image_infos.push_back({material.config.textures.albedo_sampler,
                                     material.config.textures.albedo_image->get_image_view(),
                                     VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});
              image_infos.push_back({material.config.textures.metal_rough_sampler,
                                     material.config.textures.metal_rough_image->get_image_view(),
                                     VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});
              image_infos.push_back({material.config.textures.normal_sampler,
                                     material.config.textures.normal_image->get_image_view(),
                                     VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});
                  image_infos.push_back({material.config.textures.occlusion_sampler,
                                     material.config.textures.occlusion_image->get_image_view(),
                                     VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});
              image_infos.push_back({material.config.textures.emissive_sampler,
                                         material.config.textures.emissive_image->get_image_view(),
                                     VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});
            }
              descriptor_buffer->write_image_array(info.binding_index, info.descriptor_type,
                                                   image_infos);
            image_array_set = info.set_index;
            image_array_binding_ = binding;
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

        // Process tlas bindings for this set index
        if (auto it = tlas_bindings_by_set.find(set_index); it != tlas_bindings_by_set.end()) {
          for (const auto& binding : it->second) {
            const auto& info = binding.info;
            descriptor_buffer->write_acceleration_structure(info.binding_index, binding.resource->getAddress());
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

        if (image_array_binding_.has_value() && image_array_binding_.value().resource->should_rebuild_descriptors()) {
        std::vector<VkDescriptorImageInfo> image_infos;
        const auto& info = image_array_binding_.value().info;
        for (const auto& material : image_array_binding_.value().resource->get_materials()) {
          image_infos.push_back({material.config.textures.albedo_sampler,
                                 material.config.textures.albedo_image->get_image_view(),
                                 VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});
          image_infos.push_back({material.config.textures.metal_rough_sampler,
                                 material.config.textures.metal_rough_image->get_image_view(),
                                 VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});
          image_infos.push_back({material.config.textures.normal_sampler,
                                 material.config.textures.normal_image->get_image_view(),
                                 VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});
          image_infos.push_back({material.config.textures.occlusion_sampler,
                                 material.config.textures.occlusion_image->get_image_view(),
                                 VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});
          image_infos.push_back({material.config.textures.emissive_sampler,
                                 material.config.textures.emissive_image->get_image_view(),
                                 VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});
        }
        descriptor_buffers_.at(image_array_set)
            ->write_image_array(info.binding_index, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_infos)
            .update();
        }

        std::vector<VkDescriptorBufferBindingInfoEXT> buffer_bindings;
        buffer_bindings.reserve(descriptor_buffers_.size());
        for (const auto& descriptor_buffer : descriptor_buffers_ | std::views::values) {
          buffer_bindings.push_back({.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
                                     .address = descriptor_buffer->get_address(),
                                     .usage = descriptor_buffer->get_usage()});
        }

        // TODO bind them once at the start of the frame
        cmd.bind_descriptor_buffers_ext(buffer_bindings.size(), buffer_bindings.data());

      for (const auto& [set_index, descriptor_buffer] : descriptor_buffers_) {
        descriptor_buffer->bind_descriptors(cmd.get(), bind_point, pipeline_layout_, set_index);
      }
    }
 
    [[nodiscard]] VkPipelineLayout get_pipeline_layout() const { return pipeline_layout_; }
    [[nodiscard]] VkPipeline get_pipeline() const { return pipeline_; }
    [[nodiscard]] auto get_descriptor_buffers() const { return descriptor_buffers_; }
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

      GraphicsPipelineBuilder& set_depth_format(
          const std::shared_ptr<ImageInstance>& depth_attachment) {
        render_info_.depthAttachmentFormat = depth_attachment->get_format();
        return *this;
      }

      GraphicsPipelineBuilder& set_color_attachment_formats(
          const std::map<unsigned int, std::shared_ptr<ImageInstance>>& color_attachments) {
        for (const auto& attachment : color_attachments | std::views::values) {
          color_attachmentformats_.push_back(attachment->get_format());
        }
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

  class GraphicsPipeline: Moveable<GraphicsPipeline> {
    PipelineTool pipeline_tool_;
    VkPipelineBindPoint bind_point_ = VK_PIPELINE_BIND_POINT_GRAPHICS;
    uint32_t color_attachment_count_;
    VkFormat depth_format_;
    VkFormat stencil_format_;

    void set_attachments_for_validation(const VkGraphicsPipelineCreateInfo& pipelineInfo) {
      const void* current = pipelineInfo.pNext;

      while (current) {
        const auto base = static_cast<const VkBaseInStructure*>(current);

        if (base->sType == VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO) {
          const auto rendering_info = static_cast<const VkPipelineRenderingCreateInfo*>(current);

          // Access the number of color attachments and depth/stencil formats
          color_attachment_count_ = rendering_info->colorAttachmentCount;
          depth_format_ = rendering_info->depthAttachmentFormat;
          stencil_format_ = rendering_info->stencilAttachmentFormat;
          return;
        }

        // Move to the next structure in the chain
        current = base->pNext;
      }
    }

  public:
         
    GraphicsPipeline(
        IGpu* gpu, const std::string_view pipeline_name,
        const std::span<const ResourceBinding<ImageInstance>> image_bindings,
        const std::span<const ResourceBinding<BufferInstance>> buffer_bindings,
        const std::span<const ResourceBinding<ImageArrayInstance>> image_array_bindings,
        const VkPushConstantRange push_constant_range, std::string&& task_source,
        std::string&& mesh_source, std::string&& fragment_source,
        VkGraphicsPipelineCreateInfo&& pipeline_create_info)
        : pipeline_tool_(gpu, pipeline_name) {
      pipeline_tool_.create_descriptor_layout(image_bindings, buffer_bindings,
                                              image_array_bindings);
      pipeline_tool_.add_shader(std::move(task_source), VK_SHADER_STAGE_TASK_BIT_EXT);
      pipeline_tool_.add_shader(std::move(mesh_source), VK_SHADER_STAGE_MESH_BIT_EXT);
      pipeline_tool_.add_shader(std::move(fragment_source), VK_SHADER_STAGE_FRAGMENT_BIT);
      pipeline_tool_.create_pipeline_layout(push_constant_range);
      pipeline_tool_.create_graphics_pipeline(
          pipeline_create_info, {VK_SHADER_STAGE_TASK_BIT_EXT, VK_SHADER_STAGE_MESH_BIT_EXT,
                                 VK_SHADER_STAGE_FRAGMENT_BIT});

      set_attachments_for_validation(pipeline_create_info);
    }
         
    GraphicsPipeline(
        IGpu* gpu, const std::string_view pipeline_name,
        const std::span<const ResourceBinding<ImageInstance>> image_bindings,
        const std::span<const ResourceBinding<BufferInstance>> buffer_bindings,
        const std::span<const ResourceBinding<ImageArrayInstance>> image_array_bindings,
        const VkPushConstantRange push_constant_range, std::string&& vertex_source,
        std::string&& fragment_source, VkGraphicsPipelineCreateInfo&& pipeline_create_info)
        : pipeline_tool_(gpu, pipeline_name) {
      pipeline_tool_.create_descriptor_layout(image_bindings, buffer_bindings,
                                              image_array_bindings);
      pipeline_tool_.add_shader(std::move(vertex_source), VK_SHADER_STAGE_VERTEX_BIT);
      pipeline_tool_.add_shader(std::move(fragment_source), VK_SHADER_STAGE_FRAGMENT_BIT);
      pipeline_tool_.create_pipeline_layout(push_constant_range);
      pipeline_tool_.create_graphics_pipeline(
          pipeline_create_info, {VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT});

      set_attachments_for_validation(pipeline_create_info);
    }

    void bind(const CommandBuffer cmd) {
      pipeline_tool_.bind_descriptors(cmd, bind_point_);
      cmd.bind_pipeline(bind_point_, pipeline_tool_.get_pipeline());
    }

    void begin_render_pass(
        const CommandBuffer cmd,
        const std::map<uint32, std::shared_ptr<ImageInstance>>& color_attachments,
        const std::shared_ptr<ImageInstance>& depth_attachment,
        std::optional<VkClearColorValue> clear_color = std::nullopt,
        std::optional<VkClearDepthStencilValue> clear_depth = std::nullopt) {

      if (color_attachments.empty() && depth_attachment == nullptr) {
        throw std::runtime_error("At least one attachment must be provided!");
      }

      if (color_attachments.size() > color_attachment_count_) {
        throw std::runtime_error("Color attachment was not specified in Pipeline!");
      }
      if (depth_attachment != nullptr && depth_format_ == VK_FORMAT_UNDEFINED) {
        throw std::runtime_error("Depth attachment was not specified in Pipeline!");
      }

      std::vector<VkRenderingAttachmentInfo> color_attachment_info;
      color_attachment_info.reserve(color_attachments.size());

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

      // Process color attachments
      for (const auto& attachment : color_attachments | std::views::values) {
        if (clear_color) {
          VkClearValue clear_value = {.color = clear_color.value()};
          color_attachment_info.push_back(vkinit::attachment_info(
              attachment->get_image_view(), &clear_value, attachment->get_layout()));
        } else {
          color_attachment_info.push_back(vkinit::attachment_info(
              attachment->get_image_view(), nullptr, attachment->get_layout()));
        }

        // Validate and set extent
        if (extent.width == 0 && extent.height == 0) {
          extent = {.width= attachment->get_extent().width, .height= attachment->get_extent().height};
        } else if (extent.width != attachment->get_extent().width
                   || extent.height != attachment->get_extent().height) {
          throw std::runtime_error("Attachment dimensions mismatch!");
        }
      }

      // Process depth attachment
      if (depth_attachment != nullptr) {
        if (clear_depth) {
          VkClearValue clear_value = {.depthStencil = clear_depth.value()};
          depth_attachment_info
              = vkinit::attachment_info(depth_attachment->get_image_view(), &clear_value,
                                        depth_attachment->get_layout());
        } else {
          depth_attachment_info = vkinit::attachment_info(depth_attachment->get_image_view(),
                                                          nullptr, depth_attachment->get_layout());
        }

        // Validate and set extent
        if (extent.width == 0 && extent.height == 0) {
          extent = {depth_attachment->get_extent().width, depth_attachment->get_extent().height};
        } else if (extent.width != depth_attachment->get_extent().width
                   || extent.height != depth_attachment->get_extent().height) {
          throw std::runtime_error("Attachment dimensions mismatch!");
        }
      }

      // Configure viewport and scissor
      viewport.width = static_cast<float>(extent.width);
      viewport.height = static_cast<float>(extent.height);
      scissor.extent.width = extent.width;
      scissor.extent.height = extent.height;

      // Configure rendering info
      VkRenderingInfo rendering_info = {};
      if (color_attachment_info.empty()) {
        rendering_info = vkinit::rendering_info(
            extent, nullptr, depth_attachment ? &depth_attachment_info : nullptr);
      } else {
        rendering_info = vkinit::rendering_info_for_gbuffer(
            extent, color_attachment_info.data(),
            static_cast<uint32_t>(color_attachment_info.size()),
            depth_attachment ? &depth_attachment_info : nullptr);
      }

      // Begin rendering
      cmd.begin_rendering(rendering_info);
      cmd.set_viewport(0, 1, &viewport);
      cmd.set_scissor(0, 1, &scissor);
    }


    [[nodiscard]] VkPipelineLayout get_pipeline_layout() const {
      return pipeline_tool_.get_pipeline_layout();
    }

    [[nodiscard]] VkPipeline get_pipeline() const { return pipeline_tool_.get_pipeline(); }

    [[nodiscard]] VkPipelineBindPoint get_bind_point() const { return bind_point_; }

            [[nodiscard]] auto get_descriptor_buffers() const {
      return pipeline_tool_.get_descriptor_buffers();
    }

  };

  class ComputePipeline : Moveable<ComputePipeline> {
    PipelineTool pipeline_tool_;
    VkPipelineBindPoint bind_point_ = VK_PIPELINE_BIND_POINT_COMPUTE;

  public:

    ComputePipeline(IGpu* gpu, const std::string_view pipeline_name,
                    const std::span<const ResourceBinding<ImageInstance>> image_bindings,
                    const std::span<const ResourceBinding<BufferInstance>> buffer_bindings,
                    const std::span<const ResourceBinding<ImageArrayInstance>> image_array_bindings,
                    const VkPushConstantRange push_constant_range,
                    const std::string&& compute_source,
                    const std::span<const ResourceBinding<AccelerationStructureInstance>> tlas_bindings = {})
      : pipeline_tool_(gpu, pipeline_name) {
      pipeline_tool_.create_descriptor_layout(image_bindings, buffer_bindings, image_array_bindings, tlas_bindings);
      pipeline_tool_.add_shader(std::move(compute_source), VK_SHADER_STAGE_COMPUTE_BIT);
      pipeline_tool_.create_pipeline_layout(push_constant_range);
      pipeline_tool_.create_compute_pipeline();
    }

    ComputePipeline(const ComputePipeline&) = delete;
    ComputePipeline& operator=(const ComputePipeline&) = delete;

    ComputePipeline(ComputePipeline&&) noexcept = default;
    ComputePipeline& operator=(ComputePipeline&&) noexcept = default;

    void bind(const CommandBuffer cmd) {
      pipeline_tool_.bind_descriptors(cmd, bind_point_);
      cmd.bind_pipeline(bind_point_, pipeline_tool_.get_pipeline());
    }

    [[nodiscard]] VkPipelineLayout get_pipeline_layout() const {
      return pipeline_tool_.get_pipeline_layout();
    }

    [[nodiscard]] VkPipeline get_pipeline() const { return pipeline_tool_.get_pipeline(); }

    [[nodiscard]] VkPipelineBindPoint get_bind_point() const { return bind_point_; }

    [[nodiscard]] auto get_descriptor_buffers() const {
      return pipeline_tool_.get_descriptor_buffers();
    }
  };
} // namespace gestalt
