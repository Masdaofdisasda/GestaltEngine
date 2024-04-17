#pragma once

#include "vk_types.h"
#include <string>

#include "FrameGraph.h"

namespace gestalt {
  namespace graphics {
    class LightingPass final : public RenderPass {

      VkPushConstantRange push_constant_range_{
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .offset = 0,
          .size = sizeof(RenderConfig::LightingParams),
      };
      descriptor_writer writer_;
      VkDescriptorSet descriptor_set_ = nullptr;
    public:
      void prepare() override;
      void cleanup() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Lighting Pass"; }
    };
  }  // namespace render
}  // namespace gestalt
