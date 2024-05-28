#include "vk_sync.hpp"

#include "vk_initializers.hpp"

namespace gestalt::graphics {
  void vk_sync::init(const std::shared_ptr<IGpu>& gpu, std::vector<FrameData>& frames) {
      gpu_ = gpu;

      // create synchronization structures
      // one fence to control when the gpu has finished rendering the frame,
      // and 2 semaphores to synchronize rendering with swapchain
      // we want the fence to start signalled, so we can wait on it on the first frame
      VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
      VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

      for (auto& frame : frames) {
        VK_CHECK(vkCreateFence(gpu_->getDevice(), &fenceCreateInfo, nullptr, &frame.render_fence));

        VK_CHECK(vkCreateSemaphore(gpu_->getDevice(), &semaphoreCreateInfo, nullptr,
                                   &frame.swapchain_semaphore));
        VK_CHECK(vkCreateSemaphore(gpu_->getDevice(), &semaphoreCreateInfo, nullptr,
                                   &frame.render_semaphore));

        //deletion_service_.push(frame.render_fence);
        //deletion_service_.push(frame.swapchain_semaphore);
        //deletion_service_.push(frame.render_semaphore);
      }

      VK_CHECK(vkCreateFence(gpu_->getDevice(), &fenceCreateInfo, nullptr, &imgui_fence));

      //deletion_service_.push(imgui_fence);
    }
}  // namespace gestalt