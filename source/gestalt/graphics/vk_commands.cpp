#include "vk_commands.h"

#include "vk_initializers.h"

namespace gestalt {

  using namespace graphics;

  void VkCommand::init(const Gpu& gpu, std::vector<FrameData>& frames) {
    gpu_ = gpu;
    deletion_service_.init(gpu_.device, gpu_.allocator);

    // create a command pool for commands submitted to the graphics queue.
    // we also want the pool to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(
        gpu_.graphics_queue_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (auto& frame : frames) {
      VK_CHECK(vkCreateCommandPool(gpu_.device, &commandPoolInfo, nullptr, &frame.command_pool));

      // allocate the default command buffer that we will use for rendering
      VkCommandBufferAllocateInfo cmdAllocInfo
          = vkinit::command_buffer_allocate_info(frame.command_pool, 1);

      VK_CHECK(vkAllocateCommandBuffers(gpu_.device, &cmdAllocInfo, &frame.main_command_buffer));

      deletion_service_.push(frame.command_pool);
    }

    VK_CHECK(vkCreateCommandPool(gpu_.device, &commandPoolInfo, nullptr, &imgui_command_pool));

    // allocate the command buffer for immediate submits
    VkCommandBufferAllocateInfo cmdAllocInfo
        = vkinit::command_buffer_allocate_info(imgui_command_pool, 1);

    VK_CHECK(vkAllocateCommandBuffers(gpu_.device, &cmdAllocInfo, &imgui_command_buffer));

    deletion_service_.push(imgui_command_pool);
  }

  void VkCommand::cleanup() { deletion_service_.flush(); }
}  // namespace gestalt