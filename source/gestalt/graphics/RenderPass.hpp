#pragma once

#include "FrameGraph.hpp"
#include "vk_types.hpp"

namespace gestalt::graphics {
    class DirectionalDepthPass final : public RenderPass {
    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Direction Depth Pass"; }
    };
    class DeferredPass final : public RenderPass {
    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Deferred Pass"; }
    };
    class LightingPass final : public RenderPass {
      descriptor_writer writer_;
      VkDescriptorSet descriptor_set_ = nullptr;

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
    class BrightPass final : public RenderPass {
      descriptor_writer writer;
      VkDescriptorSet descriptor_set_ = nullptr;

    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Bright Pass"; }
    };

    class BloomBlurPass final : public RenderPass {
      descriptor_writer writer;
      VkDescriptorSet descriptor_set_ = nullptr;

    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Bloom Blur Pass"; }
    };

    class LuminancePass final : public RenderPass {
      descriptor_writer writer;
      VkDescriptorSet descriptor_set_ = nullptr;

    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Luminance Pass"; }
    };
    class LuminanceDownscalePass final : public RenderPass {
      descriptor_writer writer;
      VkDescriptorSet descriptor_set_ = nullptr;

    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Luminance Downscale Pass"; }
    };

    class LightAdaptationPass final : public RenderPass {
      descriptor_writer writer;
      VkDescriptorSet descriptor_set_ = nullptr;

    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Light Adaption Pass"; }
    };

    class TonemapPass final : public RenderPass {
      descriptor_writer writer;
      VkDescriptorSet descriptor_set_ = nullptr;

    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Tonemap Pass"; }
    };
}  // namespace gestalt