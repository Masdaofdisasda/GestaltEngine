#pragma once

#include "Repository.hpp"
#include "BaseSystem.hpp"

namespace gestalt::application {

    class MaterialSystem final : public BaseSystem, public NonCopyable<MaterialSystem> {
      std::array<VkDescriptorImageInfo, getMaxTextures()> image_infos_ = {};

      void create_buffers();
      void create_default_material();
      void fill_uniform_buffer();
      void fill_images_buffer();
      void write_material(PbrMaterial& material, uint32 material_id);

    public:
      void prepare() override;
      void update(float delta_time, const UserInput& movement, float aspect) override;
      void cleanup() override;
    };

}  // namespace gestalt