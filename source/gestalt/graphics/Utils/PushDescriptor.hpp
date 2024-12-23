#pragma once 
#include <VulkanTypes.hpp>

namespace gestalt::graphics {

  struct PushDescriptor {
    PushDescriptor(const uint32_t size, const VkShaderStageFlags stage_flags) {
      push_constant_range.size = size;
      push_constant_range.offset = 0;
      push_constant_range.stageFlags = stage_flags;
    }

    PushDescriptor() {
      push_constant_range.size = 0;
      push_constant_range.offset = 0;
      push_constant_range.stageFlags = 0;
    }

    [[nodiscard]] VkPushConstantRange get_push_constant_range() const { return push_constant_range; }

  private:
    VkPushConstantRange push_constant_range;
  };

}  // namespace gestalt