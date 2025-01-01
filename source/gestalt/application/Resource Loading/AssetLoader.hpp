#pragma once

#include <filesystem>
#include "common.hpp"
#include "ECS/ComponentFactory.hpp"

namespace gestalt::foundation {
  class ImageInstance;
  class IResourceAllocator;
}

namespace fastgltf {
  struct Material;
  struct Image;
  class Asset;
}

namespace gestalt::foundation {
  struct PbrMaterial;
  class Repository;
}

namespace gestalt::application {

    class AssetLoader : public NonCopyable<AssetLoader> {
      IResourceAllocator* resource_allocator_ = nullptr;
      Repository* repository_ = nullptr;
      ComponentFactory* component_factory_ = nullptr;

      std::optional<fastgltf::Asset> parse_gltf(const std::filesystem::path& file_path);
      std::shared_ptr<ImageInstance> load_image(fastgltf::Asset& asset,
                                              fastgltf::Image& image) const;
      void import_textures(fastgltf::Asset& gltf) const;
      size_t create_material(const PbrMaterial& config, const std::string& name) const;
      std::shared_ptr<ImageInstance> get_textures(const fastgltf::Asset& gltf,
                                                        const size_t& texture_index,
                                                        const size_t& image_offset) const;
      void import_albedo(const fastgltf::Asset& gltf,
                         const size_t& image_offset, const fastgltf::Material& mat,
                         PbrMaterial& pbr_config) const;
      void import_metallic_roughness(const fastgltf::Asset& gltf,
                                     const size_t& image_offset, const fastgltf::Material& mat,
                                     PbrMaterial& pbr_config) const;
      void import_normal(const fastgltf::Asset& gltf, 
                         const size_t& image_offset, const fastgltf::Material& mat,
                         PbrMaterial& pbr_config) const;
      void import_emissive(const fastgltf::Asset& gltf, 
                           const size_t& image_offset, const fastgltf::Material& mat,
                           PbrMaterial& pbr_config) const;
      void import_occlusion(const fastgltf::Asset& gltf, 
                            const size_t& image_offset, const fastgltf::Material& mat,
                            PbrMaterial& pbr_config) const;
      void import_material(fastgltf::Asset& gltf, size_t& image_offset,
                           fastgltf::Material& mat) const;
      void import_materials(fastgltf::Asset& gltf,
                            size_t& image_offset) const;
      void import_meshes(fastgltf::Asset& gltf, size_t material_offset) const;

    public:
      void init(IResourceAllocator* resource_allocator,
                ComponentFactory* component_factory,
                Repository* repository);
      void import_nodes(fastgltf::Asset& gltf) const;
      void load_scene_from_gltf(const std::filesystem::path& file_path);
      void import_animations(const fastgltf::Asset& gltf, const size_t node_offset);
    };

}  // namespace gestalt::application