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

  class RenderEngine {
    IGpu& gpu_;
    Window& window_;
    ResourceAllocator& resource_allocator_;
    Repository& repository_;
    Gui* imgui_ = nullptr;
    FrameProvider& frame_;

      RenderConfig config_{};

      std::unique_ptr<VkSwapchain> swapchain_;

    std::unique_ptr<FrameGraph> frame_graph_;
      std::shared_ptr<ImageInstance> scene_final_;

    std::unique_ptr<SamplerInstance> post_process_sampler_;
    std::unique_ptr<SamplerInstance> interpolation_sampler_;
      std::unique_ptr<SamplerInstance> cube_map_sampler_;


      bool resize_requested_{false};
      uint32 swapchain_image_index_{0};
      FrameData frame_data_{};

      auto& frame() { return frame_data_.get_current_frame(); }

      CommandBuffer start_draw();

    public:
      RenderEngine(IGpu& gpu, Window& window, ResourceAllocator& resource_allocator,
                   Repository& repository, Gui* imgui_gui, FrameProvider& frame);
      ~RenderEngine();

      RenderEngine(const RenderEngine&) = delete;
      RenderEngine& operator=(const RenderEngine&) = delete;

      RenderEngine(RenderEngine&&) = delete;
      RenderEngine& operator=(RenderEngine&&) = delete;

      void execute_passes();

      RenderConfig& get_config() { return config_; }
      VkFormat get_swapchain_format() const { return swapchain_->swapchain_image_format; }
    };

}  // namespace gestalt