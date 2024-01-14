#pragma once 
#include <vk_types.h>

class PipelineBuilder {
public:
  std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;

  VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
  VkPipelineRasterizationStateCreateInfo _rasterizer;
  VkPipelineColorBlendAttachmentState _colorBlendAttachment;
  VkPipelineMultisampleStateCreateInfo _multisampling;
  VkPipelineLayout _pipelineLayout;
  VkPipelineDepthStencilStateCreateInfo _depthStencil;
  VkPipelineRenderingCreateInfo _renderInfo;
  VkFormat _colorAttachmentformat;

  PipelineBuilder() { clear(); }

  void clear();

  VkPipeline build_pipeline(VkDevice device);

  PipelineBuilder& set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
  PipelineBuilder& set_input_topology(VkPrimitiveTopology topology);
  PipelineBuilder& set_polygon_mode(VkPolygonMode mode);
  PipelineBuilder& set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace);
  PipelineBuilder& set_multisampling_none();
  PipelineBuilder& disable_blending();
  PipelineBuilder& enable_blending_additive();
  PipelineBuilder& enable_blending_alphablend();

  PipelineBuilder& set_color_attachment_format(VkFormat format);
  PipelineBuilder& set_depth_format(VkFormat format);
  PipelineBuilder& disable_depthtest();
  PipelineBuilder& enable_depthtest(bool depthWriteEnable, VkCompareOp op);

  PipelineBuilder& set_pipeline_layout(VkPipelineLayout layout);
};

namespace vkutil {

  void load_shader_module(const char* filePath, VkDevice device,
                                  VkShaderModule* outShaderModule);
};