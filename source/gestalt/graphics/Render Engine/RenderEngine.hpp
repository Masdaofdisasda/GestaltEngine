#pragma once
#include <memory>

#include "EngineConfiguration.hpp"
#include "FrameGraph.hpp"
#include "RenderConfig.hpp"
#include "Swapchain.hpp"
#include "common.hpp"

namespace gestalt::application {
  class Gui;
  class Window;
}

namespace gestalt::foundation {
  struct FrameProvider;
  class Repository;
  class IGpu;
}

namespace gestalt::graphics {

  class ResourceAllocator;
  class VkSwapchain;

  class FrameData {
  public:
    struct FrameResources {
      VkSemaphore swapchain_semaphore, render_semaphore;
      VkFence render_fence;

      VkCommandPool command_pool;
      VkCommandBuffer main_command_buffer;
    };

    void init(VkDevice device, uint32 graphics_queue_family_index, FrameProvider& frame);
    void cleanup(VkDevice device);
    FrameResources& get_current_frame();
    bool acquire_next_image(VkDevice device, VkSwapchainKHR& swapchain, uint32& swapchain_image_index) const;
    bool present(CommandBuffer cmd, VkQueue graphics_queue, VkSwapchainKHR& swapchain, uint32& swapchain_image_index) const;

  private:
    std::array<FrameResources, getFramesInFlight()> frames_{};
    FrameProvider* frame_ = nullptr;
  };

  class RenderEngine : public NonCopyable<RenderEngine> {
    IGpu* gpu_ = nullptr;
    Window* window_ = nullptr;
    ResourceAllocator* resource_allocator_ = nullptr;
      Repository* repository_ = nullptr;
      Gui* imgui_ = nullptr;
      FrameProvider* frame_ = nullptr;

      RenderConfig config_{};

      std::unique_ptr<VkSwapchain> swapchain_;

    std::unique_ptr<fg::FrameGraph> frame_graph_;
      std::shared_ptr<ImageInstance> scene_final_;

      std::shared_ptr<TextureHandleOld> debug_texture_;

      bool resize_requested_{false};
      uint32 swapchain_image_index_{0};
      FrameData frame_data_{};

      auto& frame() { return frame_data_.get_current_frame(); }

      CommandBuffer start_draw();

    public:
      void init(IGpu* gpu, Window* window,
                ResourceAllocator* resource_allocator,
                Repository* repository, Gui* imgui_gui, FrameProvider* frame);
      void execute_passes();

      void cleanup();

      RenderConfig& get_config() { return config_; }
      VkFormat get_swapchain_format() const { return swapchain_->swapchain_image_format; }
      std::shared_ptr<TextureHandleOld> get_debug_image() const { return debug_texture_; }
    };

}  // namespace gestalt