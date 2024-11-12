#pragma once
#include <vector>

#include "ResourceManager.hpp"
#include "ResourceTypes.hpp"
#include "common.hpp"
#include "VulkanTypes.hpp"
#include "Utils/vk_pipelines.hpp"

namespace gestalt::foundation {
  class Repository;
  struct DescriptorBuffer;
  struct FrameProvider;
  class IGpu;
}

namespace gestalt::graphics {
  struct RenderPassDependency;
  class ResourceRegistry;

  class RenderPassBase : public NonCopyable<RenderPassBase>{
    public:
      RenderPassBase() = default;
      void init(IGpu* gpu,
                ResourceManager* resource_manager,
                ResourceRegistry* registry,
                Repository* repository, FrameProvider* frame);
      RenderPassDependency& get_dependencies();

      virtual ~RenderPassBase() = default;

      virtual void execute(VkCommandBuffer cmd) = 0;
      [[nodiscard]] virtual std::string get_name() const = 0;
      virtual void cleanup();

    protected:
      virtual void prepare() = 0;
      virtual void destroy() = 0;

      void begin_renderpass(VkCommandBuffer cmd);
      static void bind_descriptor_buffers(VkCommandBuffer cmd, const std::vector<DescriptorBuffer*>& descriptor_buffers);
      void begin_compute_pass(VkCommandBuffer cmd);

      PipelineBuilder create_pipeline();
      void create_pipeline_layout();

      RenderPassDependency dependencies_{};

      VkViewport viewport_{
          .x = 0,
          .y = 0,
          .minDepth = 0.f,
          .maxDepth = 1.f,
      };
      VkRect2D scissor_{
          .offset = {0, 0},
      };

      VkPipeline pipeline_ = nullptr;
      VkPipelineLayout pipeline_layout_ = nullptr;
      std::vector<VkDescriptorSetLayout> descriptor_layouts_;
      VkSampler post_process_sampler = nullptr;

      FrameProvider* frame_;
      IGpu* gpu_;
      ResourceManager* resource_manager_;
      ResourceRegistry* registry_;
      Repository* repository_;
    };

}  // namespace gestalt