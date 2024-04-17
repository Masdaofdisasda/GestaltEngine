#pragma once

#include "FrameGraph.h"
#include "vk_types.h"

namespace gestalt {
  namespace graphics {
    class DirectionalDepthPass final : public RenderPass {
    public:
      void prepare() override;
      void cleanup() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Direction Depth Pass"; }
    };

  }  // namespace graphics
}  // namespace gestalt