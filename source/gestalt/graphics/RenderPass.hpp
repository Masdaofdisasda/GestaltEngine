#pragma once

#include "RenderEngine.hpp"
#include "vk_types.hpp"

namespace gestalt::graphics {
    class DirectionalDepthPass final : public RenderPass {
    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Direction Depth Pass"; }
    };
    class DrawCullPass final : public RenderPass {
      public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Draw Cull Pass"; }
    };
    class TaskSubmitPass final : public RenderPass {
      public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Task Submit Pass"; }
    };
    class MeshletPass final : public RenderPass {
    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Meshlet Pass"; }
    };
    class LightingPass final : public RenderPass {
      std::array<std::shared_ptr<DescriptorBuffer>, getFramesInFlight()> descriptor_buffers_;

    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Lighting Pass"; }
    };
    class SkyboxPass final : public RenderPass {
    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Skybox Pass"; }
    };
    class InfiniteGridPass final : public RenderPass {
    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Infinite Grid Pass"; }
    };
    class AabbBvhDebugPass final : public RenderPass {
    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "AABB BVH Debug Pass"; }
    };
    class BoundingSphereDebugPass final : public RenderPass {
    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Bounding Sphere Debug Pass"; }
    };
    class BrightPass final : public RenderPass {
      std::array<std::shared_ptr<DescriptorBuffer>, getFramesInFlight()> descriptor_buffers_;

    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Bright Pass"; }
    };

  constexpr uint8 kMaxBloomIterations = 6;

    class BloomBlurPass final : public RenderPass {
    std::array<std::array<std::shared_ptr<DescriptorBuffer>, kMaxBloomIterations>,
               getFramesInFlight()>
          descriptor_buffers_ = {};


    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Bloom Blur Pass"; }
    };

    class LuminancePass final : public RenderPass {
      std::array<std::shared_ptr<DescriptorBuffer>, getFramesInFlight()> descriptor_buffers_;

    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Luminance Pass"; }
    };
    class LuminanceDownscalePass final : public RenderPass {
      std::array<std::array<std::shared_ptr<DescriptorBuffer>, 10>, getFramesInFlight()>
          descriptor_buffers_;

    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Luminance Downscale Pass"; }
    };

    class LightAdaptationPass final : public RenderPass {
      std::array<std::shared_ptr<DescriptorBuffer>, getFramesInFlight()> descriptor_buffers_;

    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Light Adaption Pass"; }
    };

    class TonemapPass final : public RenderPass {
      std::array<std::shared_ptr<DescriptorBuffer>, getFramesInFlight()> descriptor_buffers_;

    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Tonemap Pass"; }
    };
}  // namespace gestalt