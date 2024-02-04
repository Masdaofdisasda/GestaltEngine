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
  void import_textures(fastgltf::Asset& gltf);
  size_t create_material(const gltf_material& material, const pbr_config& config,
                         const std::string& name) const;
  void import_materials(fastgltf::Asset& gltf, size_t& material_offset) const;
  size_t create_surface(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
  size_t create_mesh(std::vector<size_t> surfaces, const std::string& name) const;
  void add_material_component(size_t surface, size_t material) const;
  void import_meshes(fastgltf::Asset& gltf, size_t material_offset);

public:
  void init(const vk_gpu& gpu,
            const std::shared_ptr<resource_manager>& resource_manager);
  std::vector<fastgltf::Node> load_scene_from_gltf(const std::string& file_path);
};
