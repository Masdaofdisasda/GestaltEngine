#pragma once

#include "FrameGraph.h"
#include "vk_types.h"
#include "vk_descriptors.h"

namespace gestalt {
  namespace graphics {
    class BrightPass final : public RenderPass {
      std::string vertex_shader_source_ = "../shaders/fullscreen.vert.spv";
      std::string fragment_shader_source_ = "../shaders/hdr_bright_pass.frag.spv";

      uint32_t effect_size_ = 512;
      VkExtent2D extent_{effect_size_, effect_size_};

      VkPushConstantRange push_constant_range{
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .offset = 0,
          .size = sizeof(RenderConfig::HdrParams),
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

      ShaderPassDependencyInfo deps_
          = {.read_resources = {{std::make_shared<ColorImageResource>("scene_ssao", 1.0f)}},
             .write_resources = {{"scene_brightness_filtered",
                                  std::make_shared<ColorImageResource>("scene_bright", extent_)}}};

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
    };

    class BloomBlurPass final : public RenderPass {
      std::string vertex_shader_source_ = "../shaders/fullscreen.vert.spv";
      std::string fragment_blur_x = "../shaders/hdr_bloom_x.frag.spv";
      std::string fragment_blur_y = "../shaders/hdr_bloom_y.frag.spv";

      uint32_t effect_size_ = 512;
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
          .write_resources = {{"scene_bloom", std::make_shared<ColorImageResource>(
                                                  "scene_brightness_filtered", extent_)},
                              {"bloom_blurred_intermediate",
                               std::make_shared<ColorImageResource>("bloom_blur_y", extent_)}},
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
    };

    class StreaksPass final : public RenderPass {
      std::string vertex_shader_source_ = "../shaders/fullscreen.vert.spv";
      std::string fragment_shader_source_ = "../shaders/hdr_streaks.frag.spv";

      uint32_t effect_size_ = 512;
      VkExtent2D extent_{effect_size_, effect_size_};

      VkPushConstantRange push_constant_range_{
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .offset = 0,
          .size = sizeof(RenderConfig::StreaksParams),
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

      ShaderPassDependencyInfo deps_
          = {.read_resources = {{std::make_shared<ColorImageResource>("scene_bloom", extent_)}},
             .write_resources = {{"scene_streak", std::make_shared<ColorImageResource>(
                                                      "bloom_blurred_intermediate", 1.0f)}}};

      foundation::TextureHandle streak_pattern;
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
    };

    class LuminancePass final : public RenderPass {
      std::string vertex_shader_source_ = "../shaders/fullscreen.vert.spv";
      std::string fragment_shader_source_ = "../shaders/to_luminance.frag.spv";

      VkExtent2D extent64_{64, 64};

      VkViewport viewport_{
          .x = 0,
          .y = 0,
          .minDepth = 0.f,
          .maxDepth = 1.f,
      };
      VkRect2D scissor_{
          .offset = {0, 0},
      };

      ShaderPassDependencyInfo deps_
          = {.read_resources = {{std::make_shared<ColorImageResource>("scene_ssao", 1.0f)}},
             .write_resources
             = {{"lum_64_final", std::make_shared<ColorImageResource>("lum_64", extent64_)}}};

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
    };
    class LuminanceDownscalePass final : public RenderPass {
      std::string vertex_shader_source_ = "../shaders/fullscreen.vert.spv";
      std::string fragment_shader_source_ = "../shaders/downscale2x2.frag.spv";

      VkExtent2D extent64_{64, 64};
      VkExtent2D extent32_{32, 32};
      VkExtent2D extent16_{16, 16};
      VkExtent2D extent8_{8, 8};
      VkExtent2D extent4_{4, 4};
      VkExtent2D extent2_{2, 2};
      VkExtent2D extent1_{1, 1};

      VkViewport viewport_{
          .x = 0,
          .y = 0,
          .minDepth = 0.f,
          .maxDepth = 1.f,
      };
      VkRect2D scissor_{
          .offset = {0, 0},
      };

      ShaderPassDependencyInfo deps_
          = {.read_resources = {{std::make_shared<ColorImageResource>("lum_64_final", extent64_)}},
             .write_resources
             = {{"lum_32_final", std::make_shared<ColorImageResource>("lum_32", extent32_)},
                {"lum_16_final", std::make_shared<ColorImageResource>("lum_16", extent16_)},
                {"lum_8_final", std::make_shared<ColorImageResource>("lum_8", extent8_)},
                {"lum_4_final", std::make_shared<ColorImageResource>("lum_4", extent4_)},
                {"lum_2_final", std::make_shared<ColorImageResource>("lum_2", extent2_)},
                {"lum_1_final", std::make_shared<ColorImageResource>("lum_1", extent1_)}}};

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
    };
    class LightAdaptationPass final : public RenderPass {
      std::string vertex_shader_source_ = "../shaders/fullscreen.vert.spv";
      std::string adaptation_fragment_shader_source_ = "../shaders/hdr_light_adaptation.frag.spv";

      VkExtent2D extent1_{1, 1};

      VkPushConstantRange push_constant_range_{
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .offset = 0,
          .size = sizeof(RenderConfig::LightAdaptationParams),
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

      ShaderPassDependencyInfo deps_
          = {.read_resources = {{std::make_shared<ColorImageResource>("lum_1_final", extent1_)}},
             .write_resources
             = {{"lum_1_current", std::make_shared<ColorImageResource>("lum_1_new", extent1_)},
                {"lum_1_adapted", std::make_shared<ColorImageResource>("lum_1_avg", extent1_)}}};

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
    };

    class TonemapPass final : public RenderPass {
      std::string vertex_shader_source_ = "../shaders/fullscreen.vert.spv";
      std::string fragment_shader_source_ = "../shaders/hdr_final.frag.spv";

      uint32_t effect_size_ = 512;
      VkExtent2D extent_{effect_size_, effect_size_};

      VkPushConstantRange push_constant_range{
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .offset = 0,
          .size = sizeof(RenderConfig::HdrParams),
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

      ShaderPassDependencyInfo deps_
          = {.read_resources
             = {{std::make_shared<ColorImageResource>("scene_streak", extent_)},
                {std::make_shared<ColorImageResource>("directional_shadow_map", extent_)},
                {std::make_shared<ColorImageResource>("scene_ssao", extent_)},
                {std::make_shared<ColorImageResource>("lum_1_adapted", 1.0f)}},
             .write_resources
             = {{"scene_final", std::make_shared<ColorImageResource>("hdr_final", 1.0f)}}};

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
    };
  }  // namespace graphics
}  // namespace gestalt