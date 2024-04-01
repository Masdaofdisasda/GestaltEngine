#pragma once

#include "vk_types.h"
#include <string>

#include "FrameGraph.h"

namespace gestalt {
  namespace graphics {

    class DeferredPass final : public RenderPass {
      std::string vertex_shader_source_ = "../shaders/geometry.vert.spv";
      std::string fragment_shader_source_ = "../shaders/geometry_deferred.frag.spv";

      ShaderPassDependencyInfo deps_ = {
          .read_resources = {},
          .write_resources
          = {{"gbuffer_1_final", std::make_shared<ColorImageResource>("gbuffer_1", 1.f)},
             {"gbuffer_2_final", std::make_shared<ColorImageResource>("gbuffer_2", 1.f)},
             {"gbuffer_3_final", std::make_shared<ColorImageResource>("gbuffer_3", 1.f)},
             {"gbuffer_depth", std::make_shared<DepthImageResource>("gbuffer_d", 1.f)}},
      };

      VkPushConstantRange push_constant_range_{
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
          .offset = 0,
          .size = sizeof(GpuDrawPushConstants),
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

    class MeshletPass final : public RenderPass {
      std::string mesh_shader_source_ = "../shaders/geometry.mesh.spv";
      std::string task_shader_source_ = "../shaders/geometry.task.spv";
      std::string fragment_shader_source_ = "../shaders/meshlet_deferred.frag.spv";

      ShaderPassDependencyInfo deps_ = {
          .read_resources = {},
          .write_resources
          = {{"gbuffer_1_final", std::make_shared<ColorImageResource>("gbuffer_1", 1.f)},
             {"gbuffer_2_final", std::make_shared<ColorImageResource>("gbuffer_2", 1.f)},
             {"gbuffer_3_final", std::make_shared<ColorImageResource>("gbuffer_3", 1.f)},
             {"gbuffer_depth", std::make_shared<DepthImageResource>("gbuffer_d", 1.f)}},
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

    class TransparentPass final : public RenderPass {
      std::string vertex_shader_source_ = "../shaders/geometry.vert.spv";
      std::string fragment_shader_source_ = "../shaders/transparent.frag.spv";

      ShaderPassDependencyInfo deps_ = {
          .read_resources = {std::make_shared<DepthImageResource>("directional_shadow_map", 1.f)},
          .write_resources = {{"scene_transparent_color",
                               std::make_shared<ColorImageResource>("scene_opaque_color", 1.f)},
                              {"scene_transparent_depth",
                               std::make_shared<DepthImageResource>("scene_opaque_depth", 1.f)}},
      };

      VkPushConstantRange push_constant_range_{
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
          .offset = 0,
          .size = sizeof(GpuDrawPushConstants),
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

    class DebugAabbPass final : public RenderPass {
      std::string vertex_shader_source_ = "../shaders/debug_aabb.vert.spv";
      std::string fragment_shader_source_ = "../shaders/debug_aabb.frag.spv";

      ShaderPassDependencyInfo deps_ = {
          .read_resources = {},
          .write_resources
          = {{"scene_debug_aabb", std::make_shared<ColorImageResource>("scene_final", 1.f)},
             {"scene_debug_aabb_depth", std::make_shared<DepthImageResource>("grid_depth", 1.f)}},
      };

      struct aabb_debug_push_constants {
        glm::vec4 min;
        glm::vec4 max;
      };

      VkPushConstantRange push_constant_range_{
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
          .offset = 0,
          .size = sizeof(aabb_debug_push_constants),
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