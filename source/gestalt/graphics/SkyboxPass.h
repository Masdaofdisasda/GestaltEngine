#pragma once

#include "vk_types.h"
#include <string>

#include "FrameGraph.h"

namespace gestalt {
  namespace graphics {
    class SkyboxPass final : public RenderPass {
    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Skybox Pass"; }
    };
    

    class InfiniteGridPass final : public RenderPass {

      VkPushConstantRange push_constant_range{
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .offset = 0,
          .size = sizeof(RenderConfig::GridParams),
      };

    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Infinite Grid Pass"; }
    };
  }  // namespace graphics
}  // namespace gestalt
