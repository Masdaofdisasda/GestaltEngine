#include "vk_pipelines.hpp"
#include <fstream>
#include "vk_initializers.hpp"

namespace gestalt {
  namespace graphics {
    VkPipeline PipelineBuilder::build_graphics_pipeline(VkDevice device) {
      // make viewport state from our stored viewport and scissor.
      // at the moment we wont support multiple viewports or scissors
      VkPipelineViewportStateCreateInfo viewportState = {};
      viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
      viewportState.pNext = nullptr;

      viewportState.viewportCount = 1;
      viewportState.scissorCount = 1;

      // setup dummy color blending. We arent using transparent objects yet
      // the blending is just "no blend", but we do write to the color attachment
      VkPipelineColorBlendStateCreateInfo colorBlending = {};
      colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
      colorBlending.pNext = nullptr;

      colorBlending.logicOpEnable = VK_FALSE;
      colorBlending.logicOp = VK_LOGIC_OP_COPY;
      colorBlending.attachmentCount = _colorBlendAttachments.size();
      colorBlending.pAttachments = _colorBlendAttachments.data();

      // completely clear VertexInputStateCreateInfo, as we have no need for it
      VkPipelineVertexInputStateCreateInfo _vertexInputInfo
          = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

      // build the actual pipeline
      // we now use all of the info structs we have been writing into into this one
      // to create the pipeline
      VkGraphicsPipelineCreateInfo pipelineInfo
          = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
      _renderInfo.colorAttachmentCount = _colorAttachmentformats.size();
      _renderInfo.pColorAttachmentFormats = _colorAttachmentformats.data();
      // connect the renderInfo to the pNext extension mechanism
      pipelineInfo.pNext = &_renderInfo;

      pipelineInfo.stageCount = static_cast<uint32>(_shaderStages.size());
      pipelineInfo.pStages = _shaderStages.data();
      pipelineInfo.pVertexInputState = &_vertexInputInfo;
      pipelineInfo.pInputAssemblyState = &_inputAssembly;
      pipelineInfo.pViewportState = &viewportState;
      pipelineInfo.pRasterizationState = &_rasterizer;
      pipelineInfo.pMultisampleState = &_multisampling;
      pipelineInfo.pColorBlendState = &colorBlending;
      pipelineInfo.pDepthStencilState = &_depthStencil;
      pipelineInfo.layout = _pipelineLayout;

      std::vector<VkDynamicState> dynamicStates;
      dynamicStates.append_range(_dynamicStates);
      dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
      dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);

      VkPipelineDynamicStateCreateInfo dynamicInfo
          = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
      dynamicInfo.pDynamicStates = dynamicStates.data();
      dynamicInfo.dynamicStateCount = dynamicStates.size();
      pipelineInfo.pDynamicState = &dynamicInfo;

      // its easy to error out on create graphics pipeline, so we handle it a bit
      // better than the common VK_CHECK case
      VkPipeline newPipeline;
      if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline)
          != VK_SUCCESS) {
        fmt::println("failed to create pipeline");
        return VK_NULL_HANDLE;  // failed to create graphics pipeline
      }

      return newPipeline;
    }

    VkPipeline PipelineBuilder::build_compute_pipeline(VkDevice device) const {
      // Create the compute pipeline create info
      VkComputePipelineCreateInfo pipelineInfo = {};
      pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
      pipelineInfo.pNext = nullptr;

      // Set the compute shader stage
      if (_shaderStages.size() != 1 || _shaderStages[0].stage != VK_SHADER_STAGE_COMPUTE_BIT) {
        fmt::println("Invalid shader stages for compute pipeline");
        return VK_NULL_HANDLE;
      }

      pipelineInfo.stage = _shaderStages[0];
      pipelineInfo.layout = _pipelineLayout;

      // Create the compute pipeline
      VkPipeline newPipeline;
      if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline)
          != VK_SUCCESS) {
        fmt::println("Failed to create compute pipeline");
        return VK_NULL_HANDLE;  // failed to create compute pipeline
      }

      return newPipeline;
    }


    PipelineBuilder& PipelineBuilder::set_shaders(VkShaderModule vertexShader,
                                                  VkShaderModule fragmentShader) {
      _shaderStages.clear();

      _shaderStages.push_back(
          vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));

      _shaderStages.push_back(
          vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));

      return *this;
    }

    PipelineBuilder& PipelineBuilder::set_shaders(VkShaderModule computeShader) {
      _shaderStages.clear();

      _shaderStages.push_back(
          vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, computeShader));

      return *this;
    }

    PipelineBuilder& PipelineBuilder::set_shaders(VkShaderModule taskShader,
                                                  VkShaderModule meshShader,
                                                  VkShaderModule fragmentShader) {
      _shaderStages.clear();

      _shaderStages.push_back(
          vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_TASK_BIT_EXT, taskShader));

      _shaderStages.push_back(
          vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_MESH_BIT_EXT, meshShader));

      _shaderStages.push_back(
          vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));

      return *this;
    }

    PipelineBuilder& PipelineBuilder::set_input_topology(VkPrimitiveTopology topology) {
      _inputAssembly.topology = topology;
      _inputAssembly.primitiveRestartEnable = VK_FALSE;

      return *this;
    }

    PipelineBuilder& PipelineBuilder::set_polygon_mode(VkPolygonMode mode) {
      _rasterizer.polygonMode = mode;
      _rasterizer.lineWidth = 1.f;

      return *this;
    }

    PipelineBuilder& PipelineBuilder::set_cull_mode(VkCullModeFlags cullMode,
                                                    VkFrontFace frontFace) {
      _rasterizer.cullMode = cullMode;
      _rasterizer.frontFace = frontFace;

      return *this;
    }

    PipelineBuilder& PipelineBuilder::set_multisampling_none() {
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

    PipelineBuilder& PipelineBuilder::disable_blending(uint32_t count) {
      VkPipelineColorBlendAttachmentState attachment = {
          .blendEnable = VK_FALSE,
          .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      };

      for (uint32_t i = 0; i < count; i++) {
        _colorBlendAttachments.push_back(attachment);
      }

      return *this;
    }

    PipelineBuilder& PipelineBuilder::enable_blending_additive() {
      VkPipelineColorBlendAttachmentState attachment = {
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

    PipelineBuilder& PipelineBuilder::enable_blending_alphablend() {
      VkPipelineColorBlendAttachmentState attachment = {
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

    PipelineBuilder& PipelineBuilder::set_color_attachment_format(VkFormat format) {
      _colorAttachmentformats.emplace_back(format);

      return *this;
    }

    PipelineBuilder& PipelineBuilder::set_color_attachment_formats(
        const std::vector<VkFormat>& formats) {
      _colorAttachmentformats.insert(_colorAttachmentformats.end(), formats.begin(), formats.end());

      return *this;
    }

    PipelineBuilder& PipelineBuilder::set_depth_format(VkFormat format) {
      _renderInfo.depthAttachmentFormat = format;

      return *this;
    }

    PipelineBuilder& PipelineBuilder::disable_depthtest() {
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

    PipelineBuilder& PipelineBuilder::enable_depthtest(bool depthWriteEnable, VkCompareOp op) {
      _depthStencil.depthTestEnable = VK_TRUE;
      _depthStencil.depthWriteEnable = depthWriteEnable;
      _depthStencil.depthCompareOp = op;
      _depthStencil.depthBoundsTestEnable = VK_FALSE;
      _depthStencil.stencilTestEnable = VK_FALSE;
      _depthStencil.front = {};
      _depthStencil.back = {};
      _depthStencil.minDepthBounds = 0.f;
      _depthStencil.maxDepthBounds = 1.f;

      return *this;
    }

    PipelineBuilder& PipelineBuilder::enable_dynamic_depth_bias() {
      _dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);

      return *this;
    }

    PipelineBuilder& PipelineBuilder::set_pipeline_layout(VkPipelineLayout layout) {
      _pipelineLayout = layout;

      return *this;
    }

    void PipelineBuilder::clear() {
      // clear all of the structs we need back to 0 with their correct stype

      _inputAssembly = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};

      _rasterizer = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};

      _colorBlendAttachments.clear();

      _multisampling = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};

      _pipelineLayout = {};

      _depthStencil = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};

      _renderInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};

      _shaderStages.clear();
    }

    void vkutil::load_shader_module(const char* filePath, VkDevice device,
                                    VkShaderModule* outShaderModule) {
      // open the file. With cursor at the end
      std::ifstream file(filePath, std::ios::ate | std::ios::binary);

      if (!file.is_open()) {
        fmt::print("Error: Could not load shader file: {}\n", filePath);
        abort();
      }

      // find what the size of the file is by looking up the location of the cursor
      // because the cursor is at the end, it gives the size directly in bytes
      size_t fileSize = (size_t)file.tellg();

      // spirv expects the buffer to be on uint32, so make sure to reserve a int
      // vector big enough for the entire file
      std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

      // put file cursor at beginning
      file.seekg(0);

      // load the entire file into the buffer
      file.read((char*)buffer.data(), fileSize);

      // now that the file is loaded into the buffer, we can close it
      file.close();

      // create a new shader module, using the buffer we loaded
      VkShaderModuleCreateInfo createInfo = {};
      createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
      createInfo.pNext = nullptr;

      // codeSize has to be in bytes, so multply the ints in the buffer by size of
      // int to know the real size of the buffer
      createInfo.codeSize = buffer.size() * sizeof(uint32_t);
      createInfo.pCode = buffer.data();

      // check that the creation goes well.
      VkShaderModule shaderModule;
      if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        fmt::print("Error: Could not load shader file: {}\n", filePath);
        abort();
      }
      *outShaderModule = shaderModule;
    }
  }  // namespace graphics
}  // namespace gestalt