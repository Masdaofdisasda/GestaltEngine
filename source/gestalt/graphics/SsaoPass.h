#pragma once

#include "FrameGraph.h"
#include "vk_types.h"
#include "vk_descriptors.h"

namespace gestalt {
  namespace graphics {
    class SsaoFilterPass final : public RenderPass {
      std::string vertex_shader_source_ = "../shaders/fullscreen.vert.spv";
      std::string fragment_shader_source_ = "../shaders/ssao_filter.frag.spv";
      std::string name_ = "SSAO Filter Pass";

      uint32_t effect_size_ = 1024;
      VkExtent2D extent_{effect_size_, effect_size_};

      VkPushConstantRange push_constant_range_{
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .offset = 0,
          .size = sizeof(RenderConfig::SsaoParams),
      };

      VkViewport viewport_{
          .x = 0,
          .y = 0,
          .width = static_cast<float>(effect_size_),
          .height = static_cast<float>(effect_size_),
          .minDepth = 0.f,
          .maxDepth = 1.f,
      };
      VkRect2D scissor_{
          .offset = {0, 0},
          .extent = extent_,
      };

      foundation::TextureHandle rotation_pattern;
      ShaderPassDependencyInfo deps_
          = {.read_resources = {{std::make_shared<DepthImageResource>("grid_depth", 1.0f)}},
             .write_resources = {{"color_ssao_filtered", std::make_shared<ColorImageResource>(
                                                             "color_ssao_filter", extent_)}}};

      descriptor_writer writer;

      VkPipeline pipeline_ = nullptr;
      VkPipelineLayout pipeline_layout_ = nullptr;
      std::vector<VkDescriptorSetLayout> descriptor_layouts_;
      VkDescriptorSet descriptor_set_ = nullptr;

    public:
      void prepare() override;
      void cleanup() override;
      void execute(VkCommandBuffer cmd) override;
      ShaderPassDependencyInfo& get_dependencies() override { return deps_; }
      std::string get_name() const override { return name_; }
    };

    class SsaoBlurPass final : public RenderPass {
      std::string vertex_shader_source_ = "../shaders/fullscreen.vert.spv";
      std::string fragment_blur_x = "../shaders/ssao_blur_x.frag.spv";
      std::string fragment_blur_y = "../shaders/ssao_blur_y.frag.spv";
      std::string name_ = "SSAO Blur Pass";

      uint32_t effect_size_ = 1024;
      VkExtent2D extent_{effect_size_, effect_size_};

      VkViewport viewport_{
          .x = 0,
          .y = 0,
          .width = static_cast<float>(effect_size_),
          .height = static_cast<float>(effect_size_),
          .minDepth = 0.f,
          .maxDepth = 1.f,
      };
      VkRect2D scissor_{
          .offset = {0, 0},
          .extent = extent_,
      };

      ShaderPassDependencyInfo deps_ = {
          .read_resources = {},
          .write_resources = {{"ssao_blurred_final", std::make_shared<ColorImageResource>(
                                                         "color_ssao_filtered", extent_)},
                              {"ssao_blurred_intermediate",
                               std::make_shared<ColorImageResource>("ssao_blur_y", extent_)}},
      };

      descriptor_writer writer;

      VkPipeline blur_x_pipeline_ = nullptr;
      VkPipeline blur_y_pipeline_ = nullptr;
      VkPipelineLayout pipeline_layout_ = nullptr;
      std::vector<VkDescriptorSetLayout> descriptor_layouts_;
      VkDescriptorSet blur_x_descriptor_set_ = nullptr;
      VkDescriptorSet blur_y_descriptor_set_ = nullptr;

    public:
      void prepare() override;
      void cleanup() override;
      void execute(VkCommandBuffer cmd) override;
      ShaderPassDependencyInfo& get_dependencies() override { return deps_; }
      std::string get_name() const override { return name_; }
    };

    class SsaoFinalPass final : public RenderPass {
      std::string vertex_shader_source_ = "../shaders/fullscreen.vert.spv";
      std::string fragment_shader_source_ = "../shaders/ssao_final.frag.spv";
      std::string name_ = "SSAO Final Pass";

      uint32_t effect_size_ = 1024;
      VkExtent2D extent_{effect_size_, effect_size_};

      VkPushConstantRange push_constant_range_{
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .offset = 0,
          .size = sizeof(RenderConfig::SsaoParams),
      };

      VkViewport viewport_{
          .x = 0,
          .y = 0,
          .minDepth = 0.f,
          .maxDepth = 1.f,
      };
      VkRect2D scissor_{
          .offset = {0, 0},
      };

      ShaderPassDependencyInfo deps_ = {
          .read_resources = {{std::make_shared<DepthImageResource>("grid_color", 1.f)},
                             {std::make_shared<DepthImageResource>("ssao_blurred_final", extent_)}},
          .write_resources
          = {{"scene_ssao", std::make_shared<ColorImageResource>("scene_ssao_final", 1.0f)}}};

      descriptor_writer writer;

      VkPipeline pipeline_ = nullptr;
      VkPipelineLayout pipeline_layout_ = nullptr;
      std::vector<VkDescriptorSetLayout> descriptor_layouts_;
      VkDescriptorSet descriptor_set_ = nullptr;

    public:
      void prepare() override;
      void cleanup() override;
      void execute(VkCommandBuffer cmd) override;
      ShaderPassDependencyInfo& get_dependencies() override { return deps_; }
      std::string get_name() const override { return name_; }
    };
  }  // namespace graphics
}  // namespace gestalt