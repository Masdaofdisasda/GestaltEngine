#pragma once
#include <memory>
#include <unordered_map>

#include "RenderConfig.hpp"
#include "ResourceTypes.hpp"
#include "common.hpp"
#include "Resources/TextureHandle.hpp"
#include "Resources/TextureType.hpp"

namespace gestalt::foundation {
  class Repository;
  class IGpu;
}

namespace gestalt::graphics {

    class ResourceRegistry : public NonCopyable<ResourceRegistry> {
    public:
      void init(IGpu* gpu, const Repository* repository);

      VkShaderModule get_shader(const ShaderProgram& shader_program);
      void clear_shader_cache();

      RenderConfig config_;

      struct RenderPassResources {
        ImageAttachment scene_color{.name = "scene_color",
                                    .image = std::make_shared<TextureHandle>(TextureType::kColor)};
        ImageAttachment final_color{.name = "final_color",
                                    .image = std::make_shared<TextureHandle>(TextureType::kColor)};
        ImageAttachment scene_depth{.name = "scene_depth",
                                    .image = std::make_shared<TextureHandle>(TextureType::kDepth)};
        ImageAttachment shadow_map{.name = "shadow_map",
                                   .image = std::make_shared<TextureHandle>(TextureType::kDepth),
                                   .extent = {2048 * 4, 2048 * 4},
                                   .format = VK_FORMAT_D32_SFLOAT};
        ImageAttachment gbuffer1{.name = "gbuffer1",
                                 .image = std::make_shared<TextureHandle>(TextureType::kColor)};
        ImageAttachment gbuffer2{.name = "gbuffer2",
                                 .image = std::make_shared<TextureHandle>(TextureType::kColor)};
        ImageAttachment gbuffer3{.name = "gbuffer3",
                                 .image = std::make_shared<TextureHandle>(TextureType::kColor)};

        ImageAttachment ambient_occlusion{
            .name = "ambient_occlusion",
            .image = std::make_shared<TextureHandle>(TextureType::kColor),
            .scale = 0.5f,
            .format = VK_FORMAT_R16_SFLOAT};
        ImageAttachment ambient_occlusion_blurred{
            .name = "ambient_occlusion_blurred",
            .image = std::make_shared<TextureHandle>(TextureType::kColor),
            .scale = 0.5f,
            .format = VK_FORMAT_R16_SFLOAT};

        ImageAttachment froxel_data_texture_0
            = {.name = "froxel_data_texture_0",
               .image = std::make_shared<TextureHandle>(TextureType::kColor),
               .extent = {128, 128, 128},
               .format = VK_FORMAT_R16G16B16A16_SFLOAT};
        ImageAttachment light_scattering_texture
            = {.name = "light_scattering_texture",
               .image = std::make_shared<TextureHandle>(TextureType::kColor),
               .extent = {128, 128, 128},
               .format = VK_FORMAT_R16G16B16A16_SFLOAT};
        ImageAttachment integrated_light_scattering_texture
            = {.name = "integrated_light_scattering_texture",
               .image = std::make_shared<TextureHandle>(TextureType::kColor),
               .extent = {128, 128, 128},
               .format = VK_FORMAT_R16G16B16A16_SFLOAT};
        ImageAttachment volumetric_noise_texture
            = {.name = "volumetric_noise_texture",
               .image = std::make_shared<TextureHandle>(TextureType::kColor),
               .extent = {64, 64, 64},
               .format = VK_FORMAT_R8_UNORM,
               .initial_value = {{1.f, 0.f, 0.f, 1.f}}};

        ImageAttachment bright_pass{.name = "bright_pass",
                                    .image = std::make_shared<TextureHandle>(TextureType::kColor),
                                    .extent = {512, 512},
                                    .format = VK_FORMAT_R16G16B16A16_SFLOAT};
        ImageAttachment blur_y{.name = "blur_y",
                               .image = std::make_shared<TextureHandle>(TextureType::kColor),
                               .extent = {512, 512},
                               .format = VK_FORMAT_R16G16B16A16_SFLOAT};

        ImageAttachment lum_64{.name = "lum_64",
                               .image = std::make_shared<TextureHandle>(TextureType::kColor),
                               .extent = {64, 64},
                               .format = VK_FORMAT_R16_SFLOAT};
        ImageAttachment lum_32{.name = "lum_32",
                               .image = std::make_shared<TextureHandle>(TextureType::kColor),
                               .extent = {32, 32},
                               .format = VK_FORMAT_R16_SFLOAT};
        ImageAttachment lum_16{.name = "lum_16",
                               .image = std::make_shared<TextureHandle>(TextureType::kColor),
                               .extent = {16, 16},
                               .format = VK_FORMAT_R16_SFLOAT};
        ImageAttachment lum_8{.name = "lum_8",
                              .image = std::make_shared<TextureHandle>(TextureType::kColor),
                              .extent = {8, 8},
                              .format = VK_FORMAT_R16_SFLOAT};
        ImageAttachment lum_4{.name = "lum_4",
                              .image = std::make_shared<TextureHandle>(TextureType::kColor),
                              .extent = {4, 4},
                              .format = VK_FORMAT_R16_SFLOAT};
        ImageAttachment lum_2{.name = "lum_2",
                              .image = std::make_shared<TextureHandle>(TextureType::kColor),
                              .extent = {2, 2},
                              .format = VK_FORMAT_R16_SFLOAT};
        ImageAttachment lum_1{.name = "lum_1",
                              .image = std::make_shared<TextureHandle>(TextureType::kColor),
                              .extent = {1, 1},
                              .format = VK_FORMAT_R16_SFLOAT};

        ImageAttachment lum_A{.name = "lum_A",
                              .image = std::make_shared<TextureHandle>(TextureType::kColor),
                              .extent = {1, 1},
                              .format = VK_FORMAT_R16_SFLOAT,
                              .initial_value = {{1e7f, 0.f, 0.f, 1.f}}};
        ImageAttachment lum_B{.name = "lum_B",
                              .image = std::make_shared<TextureHandle>(TextureType::kColor),
                              .extent = {1, 1},
                              .format = VK_FORMAT_R16_SFLOAT,
                              .initial_value = {{1e7f, 0.f, 0.f, 1.f}}};

        BufferResource perFrameDataBuffer = {};
        BufferResource materialBuffer = {};
        BufferResource lightBuffer = {};
        BufferResource meshBuffer = {};
      } resources_;

      std::vector<ImageAttachment> attachment_list_;

    private:
      IGpu* gpu_ = nullptr;
      std::unordered_map<std::string, VkShaderModule> shader_cache_{};
    };

}  // namespace gestalt