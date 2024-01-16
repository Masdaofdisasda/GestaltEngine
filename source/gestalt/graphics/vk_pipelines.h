#pragma once 
#include <vk_types.h>

#include "vk_deletion_service.h"
#include "vk_descriptors.h"
#include "vk_gpu.h"

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

struct compute_push_constants {
  glm::vec4 data1;
  glm::vec4 data2;
  glm::vec4 data3;
  glm::vec4 data4;
};

struct compute_effect {
  const char* name;

  VkPipeline pipeline;
  VkPipelineLayout layout;

  compute_push_constants data;
};

class vk_pipeline_manager {

  vk_gpu gpu_;
  vk_descriptor_manager descriptor_manager_;
  vk_deletion_service deletion_service_;

  void init_background_pipelines();

public:
  VkPipeline gradient_pipeline;
  VkPipelineLayout gradient_pipeline_layout;

  std::vector<compute_effect> background_effects;

  void init(const vk_gpu& gpu, vk_descriptor_manager& descriptor_manager);
};

namespace vkutil {

  void load_shader_module(const char* filePath, VkDevice device,
                                  VkShaderModule* outShaderModule);
};