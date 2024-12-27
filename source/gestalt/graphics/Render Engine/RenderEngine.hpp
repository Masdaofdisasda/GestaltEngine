#pragma once
#include <memory>
#include <vector>

#include "FrameGraph.hpp"
#include "RenderPassBase.hpp"
#include "ResourceRegistry.hpp"
#include "Swapchain.hpp"
#include "common.hpp"
#include "Utils/vk_descriptors.hpp"

namespace gestalt::application {
  class Gui;
  class Window;
}

namespace gestalt::foundation {
  struct RenderConfig;
  struct FrameProvider;
  class Repository;
  class IGpu;
}

namespace gestalt::graphics {

  class ResourceAllocator;
  class VkSwapchain;
  class ResourceManager;

  class RenderEngine : public NonCopyable<RenderEngine> {
    IGpu* gpu_ = nullptr;
    Window* window_ = nullptr;
      ResourceManager* resource_manager_ = nullptr;
    ResourceAllocator* resource_allocator_ = nullptr;
      Repository* repository_ = nullptr;
      Gui* imgui_ = nullptr;
      FrameProvider* frame_ = nullptr;

      std::unique_ptr<ResourceRegistry> resource_registry_;

      std::unique_ptr<VkSwapchain> swapchain_;

      std::vector<std::shared_ptr<RenderPassBase>> render_passes_;

    std::unique_ptr<fg::FrameGraph> frame_graph_;
      std::shared_ptr<ImageInstance> scene_final_;

      std::shared_ptr<TextureHandleOld> debug_texture_;

      bool resize_requested_{false};
      uint32 swapchain_image_index_{0};
      FrameData frames_{};
      bool acquire_next_image();
      void present(VkCommandBuffer cmd);

      auto& frame() { return frames_.get_current_frame(); }

      void create_resources() const;
      VkCommandBuffer start_draw();
      void execute(const std::shared_ptr<RenderPassBase>& render_pass, VkCommandBuffer cmd);

    public:
      void init(IGpu* gpu, Window* window, ResourceManager* resource_manager,
                ResourceAllocator* resource_allocator,
                Repository* repository, Gui* imgui_gui, FrameProvider* frame);
      void execute_passes();

      void cleanup();

      RenderConfig& get_config() const { return resource_registry_->config_; }
      VkFormat get_swapchain_format() const { return swapchain_->swapchain_image_format; }
      std::shared_ptr<TextureHandleOld> get_debug_image() const { return debug_texture_; }
    };

}  // namespace gestalt