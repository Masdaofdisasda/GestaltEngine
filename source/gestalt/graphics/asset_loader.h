#pragma once

#include "resource_manager.h"

class scene_manager;

class asset_loader {
  vk_gpu gpu_ = {};
  std::shared_ptr<resource_manager> resource_manager_;

  VkFilter extract_filter(fastgltf::Filter filter);
  VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter);
  std::optional<fastgltf::Asset> parse_gltf(const std::filesystem::path& file_path);
  void import_samplers(fastgltf::Asset& gltf);
  void import_textures(fastgltf::Asset& gltf) const;
  size_t create_material(const pbr_material& config,
                         const std::string& name) const;
  std::tuple<AllocatedImage, VkSampler> get_textures(const fastgltf::Asset& gltf, const size_t& texture_index, const size_t& image_offset, const size_t&
                                                     sampler_offset) const;
  void import_albedo(const fastgltf::Asset& gltf, const size_t& sampler_offset, const size_t& image_offset,
                     const fastgltf::Material& mat, pbr_material& pbr_config) const;
  void import_metallic_roughness(const fastgltf::Asset& gltf, const size_t& sampler_offset,
                                 const size_t& image_offset, const fastgltf::Material& mat,
                                 pbr_material& pbr_config) const;
  void import_normal(const fastgltf::Asset& gltf, const size_t& sampler_offset,
                     const size_t& image_offset, const fastgltf::Material& mat,
                     pbr_material& pbr_config) const;
  void import_emissive(const fastgltf::Asset& gltf, const size_t& sampler_offset,
                       const size_t& image_offset, const fastgltf::Material& mat,
                       pbr_material& pbr_config) const;
  void import_occlusion(const fastgltf::Asset& gltf, const size_t& sampler_offset,
                        const size_t& image_offset, const fastgltf::Material& mat,
                        pbr_material& pbr_config) const;
  void import_material(fastgltf::Asset& gltf, size_t& sampler_offset, size_t& image_offset,
                       fastgltf::Material& mat) const;
  void import_materials(fastgltf::Asset& gltf, size_t& sampler_offset, size_t& image_offset) const;
  size_t create_surface(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
  size_t create_mesh(std::vector<size_t> surfaces, const std::string& name) const;
  void add_material_component(size_t surface, size_t material) const;
  void import_meshes(fastgltf::Asset& gltf, size_t material_offset);

public:
  void init(const vk_gpu& gpu,
            const std::shared_ptr<resource_manager>& resource_manager);
  std::vector<fastgltf::Node> load_scene_from_gltf(const std::string& file_path);
};
