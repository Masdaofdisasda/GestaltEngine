#pragma once

#include "vk_types.h"
#include <string>

#include "FrameGraph.h"

namespace gestalt {
  namespace graphics {
    class LightingPass final : public RenderPass {
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
