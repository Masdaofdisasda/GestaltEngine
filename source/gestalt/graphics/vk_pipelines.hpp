#pragma once 
#include <vk_types.hpp>

namespace gestalt::graphics {

    class PipelineBuilder {
    public:
      std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;

      VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
      VkPipelineRasterizationStateCreateInfo _rasterizer;
      std::vector<VkPipelineColorBlendAttachmentState> _colorBlendAttachments;
      VkPipelineMultisampleStateCreateInfo _multisampling;
      VkPipelineLayout _pipelineLayout;
      VkPipelineDepthStencilStateCreateInfo _depthStencil;
      VkPipelineRenderingCreateInfo _renderInfo;
      std::vector<VkFormat> _colorAttachmentformats;
      std::vector<VkDynamicState> _dynamicStates;

      PipelineBuilder() { clear(); }

      void clear();

      VkPipeline build_graphics_pipeline(VkDevice device);
      VkPipeline build_compute_pipeline(VkDevice device) const;

      PipelineBuilder& set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
      PipelineBuilder& set_shaders(VkShaderModule computeShader);
      PipelineBuilder& set_shaders(VkShaderModule taskShader, VkShaderModule meshShader,
                                   VkShaderModule fragmentShader);
      PipelineBuilder& set_input_topology(VkPrimitiveTopology topology);
      PipelineBuilder& set_polygon_mode(VkPolygonMode mode);
      PipelineBuilder& set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace);
      PipelineBuilder& set_multisampling_none();
      PipelineBuilder& disable_blending(uint32_t count = 1);
      PipelineBuilder& enable_blending_additive();
      PipelineBuilder& enable_blending_alphablend();

      PipelineBuilder& set_color_attachment_format(VkFormat format);
      PipelineBuilder& set_color_attachment_formats(const std::vector<VkFormat>& formats);
      PipelineBuilder& set_depth_format(VkFormat format);
      PipelineBuilder& disable_depthtest();
      PipelineBuilder& enable_depthtest(bool depthWriteEnable, VkCompareOp op);
      PipelineBuilder& enable_dynamic_depth_bias();

      PipelineBuilder& set_pipeline_layout(VkPipelineLayout layout);
    };

    namespace vkutil {

      void load_shader_module(const char* filePath, VkDevice device,
                              VkShaderModule* outShaderModule);
    };

}  // namespace gestalt