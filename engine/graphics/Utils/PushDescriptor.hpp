#pragma once 
#include <VulkanCore.hpp>

namespace gestalt::graphics {

  struct PushDescriptor {
    PushDescriptor(const uint32_t size, const VkShaderStageFlags stage_flags) {
      push_constant_range_.size = size;
      push_constant_range_.offset = 0;
      push_constant_range_.stageFlags = stage_flags;
    }

    PushDescriptor() {
      push_constant_range_.size = 0;
      push_constant_range_.offset = 0;
      push_constant_range_.stageFlags = 0;
    }

    [[nodiscard]] VkPushConstantRange get_push_constant_range() const { return push_constant_range_; }

  private:
    VkPushConstantRange push_constant_range_;
  };

}  // namespace gestalt