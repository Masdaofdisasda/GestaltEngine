#pragma once

#include "VulkanTypes.hpp"
#include "Render Engine/RenderPassBase.hpp"

namespace gestalt::graphics {
    class DrawCullDirectionalDepthPass final : public RenderPassBase {
      public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Draw Cull Direction Depth Pass"; }
    };
    class TaskSubmitDirectionalDepthPass final : public RenderPassBase {
      public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Task Submit Direction Depth Pass"; }
    };
    class MeshletDirectionalDepthPass final : public RenderPassBase {
    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Meshlet Direction Depth Pass"; }
    };
    class DrawCullPass final : public RenderPassBase {
      public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Draw Cull Pass"; }
    };
    class TaskSubmitPass final : public RenderPassBase {
      public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Task Submit Pass"; }
    };
    class MeshletPass final : public RenderPassBase {
    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Meshlet Pass"; }
    };

    class VolumetricLightingInjectionPass final : public RenderPassBase {
      TextureHandle blue_noise_;
      std::array<std::shared_ptr<DescriptorBuffer>, getFramesInFlight()> descriptor_buffers_;
    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Volumetric Lighting Injection Pass"; }
    };

    class VolumetricLightingScatteringPass final : public RenderPassBase {
      TextureHandle blue_noise_;
      std::array<std::shared_ptr<DescriptorBuffer>, getFramesInFlight()> descriptor_buffers_;
    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Volumetric Lighting Scattering Pass"; }
    };

    class VolumetricLightingSpatialFilterPass final : public RenderPassBase {
      std::array<std::shared_ptr<DescriptorBuffer>, getFramesInFlight()> descriptor_buffers_;
    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Volumetric Lighting Spatial Filter Pass"; }
    };

    class VolumetricLightingTemporalFilterPass final : public RenderPassBase {
    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Volumetric Lighting Temporal Filter Pass"; }
    };

    class VolumetricLightingIntegrationPass final : public RenderPassBase {
      std::array<std::shared_ptr<DescriptorBuffer>, getFramesInFlight()> descriptor_buffers_;
    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Volumetric Lighting Integration Pass"; }
    };

    class VolumetricLightingNoisePass final : public RenderPassBase {
      std::array<std::shared_ptr<DescriptorBuffer>, getFramesInFlight()> descriptor_buffers_;
    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Volumetric Lighting Noise Baking Pass"; }
    };

    class LightingPass final : public RenderPassBase {
      std::array<std::shared_ptr<DescriptorBuffer>, getFramesInFlight()> descriptor_buffers_;

    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Lighting Pass"; }
    };


    class SkyboxPass final : public RenderPassBase {
    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Skybox Pass"; }
    };

    class InfiniteGridPass final : public RenderPassBase {
    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Infinite Grid Pass"; }
    };

    class AabbBvhDebugPass final : public RenderPassBase {
    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "AABB BVH Debug Pass"; }
    };

    class BoundingSphereDebugPass final : public RenderPassBase {
    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Bounding Sphere Debug Pass"; }
    };

    class SsaoPass final : public RenderPassBase {
      TextureHandle rotation_texture_;
      std::array<std::shared_ptr<DescriptorBuffer>, getFramesInFlight()> descriptor_buffers_;

    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "SSAO Filter Pass"; }
    };

    class SsaoBlurPass final : public RenderPassBase {
      std::array<std::shared_ptr<DescriptorBuffer>, getFramesInFlight()> descriptor_buffers_;

    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Ssao Blur Pass"; }
    };
       

    class BrightPass final : public RenderPassBase {
      std::array<std::shared_ptr<DescriptorBuffer>, getFramesInFlight()> descriptor_buffers_;

    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Bright Pass"; }
    };

  constexpr uint8 kMaxBloomBlurIterations = 6;

    class BloomBlurPass final : public RenderPassBase {
    std::array<std::array<std::shared_ptr<DescriptorBuffer>, kMaxBloomBlurIterations>,
               getFramesInFlight()>
          descriptor_buffers_ = {};


    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Bloom Blur Pass"; }
    };

    class LuminancePass final : public RenderPassBase {
      std::array<std::shared_ptr<DescriptorBuffer>, getFramesInFlight()> descriptor_buffers_;

    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Luminance Pass"; }
    };
    class LuminanceDownscalePass final : public RenderPassBase {
      std::array<std::array<std::shared_ptr<DescriptorBuffer>, 10>, getFramesInFlight()>
          descriptor_buffers_;

    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Luminance Downscale Pass"; }
    };

    class LightAdaptationPass final : public RenderPassBase {
      std::array<std::shared_ptr<DescriptorBuffer>, getFramesInFlight()> descriptor_buffers_;

    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Light Adaption Pass"; }
    };

    class TonemapPass final : public RenderPassBase {
      std::array<std::shared_ptr<DescriptorBuffer>, getFramesInFlight()> descriptor_buffers_;

    public:
      void prepare() override;
      void destroy() override;
      void execute(VkCommandBuffer cmd) override;
      std::string get_name() const override { return "Tonemap Pass"; }
    };
}  // namespace gestalt