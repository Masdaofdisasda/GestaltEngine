#pragma once

#include "FrameGraph.h"
#include "vk_types.h"
#include "vk_descriptors.h"

namespace gestalt {
  namespace graphics {

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
  }  // namespace graphics
}  // namespace gestalt