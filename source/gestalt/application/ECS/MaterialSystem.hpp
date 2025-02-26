#pragma once

#include "Repository.hpp"
#include "BaseSystem.hpp"

namespace gestalt::application {

    class MaterialSystem final {
      std::array<VkDescriptorImageInfo, getMaxTextures()> image_infos_ = {};

      IGpu& gpu_;
      IResourceAllocator& resource_allocator_;
      Repository& repository_;
      FrameProvider& frame_;

      void create_buffers();
      void create_default_material();
      void fill_uniform_buffer();
      void write_material(PbrMaterial& material, uint32 material_id);

    public:
      MaterialSystem(IGpu& gpu, IResourceAllocator& resource_allocator, Repository& repository,
                     FrameProvider& frame);
      ~MaterialSystem();

      MaterialSystem(const MaterialSystem&) = delete;
      MaterialSystem& operator=(const MaterialSystem&) = delete;

      MaterialSystem(MaterialSystem&&) = delete;
      MaterialSystem& operator=(MaterialSystem&&) = delete;

      void update();
    };

}  // namespace gestalt