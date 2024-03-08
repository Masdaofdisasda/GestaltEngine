#pragma once

#include "vk_types.h"
#include "scene_components.h"

// Component storage types
using entity_container = std::vector<entity>;
using mesh_container = std::vector<mesh_component>;
using camera_container = std::vector<camera_component>;
using light_container = std::vector<light_component>;
using material_container = std::vector<material_component>;
using transform_container = std::vector<transform_component>;

class database {
public:
  size_t add_vertices(std::vector<Vertex>& vertices) {
    const size_t offset = gpu_data_.vertices_.size();
    gpu_data_.vertices_.insert(gpu_data_.vertices_.end(), vertices.begin(), vertices.end());
    return offset;
  }
  size_t add_indices(std::vector<uint32_t>& indices) {
    const size_t offset = gpu_data_.indices_.size();
    gpu_data_.indices_.insert(gpu_data_.indices_.end(), indices.begin(), indices.end());
    return offset;
  }
  size_t add_surface(const mesh_surface& surface) {
    gpu_data_.surfaces_.push_back(surface);
    return gpu_data_.surfaces_.size() - 1;
  }
  size_t add_matrix(const glm::mat4& matrix) {
    gpu_data_.matrices_.push_back(matrix);
    return gpu_data_.matrices_.size() - 1;
  }
  size_t add_light_view_proj(const glm::mat4& matrix) {
    gpu_data_.light_view_projections.push_back(matrix);
    return gpu_data_.light_view_projections.size() - 1;
  }
  size_t add_image(const AllocatedImage& image) {
    gpu_data_.images_.push_back(image);
    return gpu_data_.images_.size() - 1;
  }
  size_t add_sampler(const VkSampler& sampler) {
    gpu_data_.samplers_.push_back(sampler);
    return gpu_data_.samplers_.size() - 1;
  }

  std::vector<Vertex>& get_vertices() { return gpu_data_.vertices_; }
  std::vector<uint32_t>& get_indices() { return gpu_data_.indices_; }
  std::vector<mesh_surface>& get_surfaces() { return gpu_data_.surfaces_; }
  std::vector<glm::mat4>& get_matrices() { return gpu_data_.matrices_; }
  std::vector<glm::mat4>& get_light_view_projs() { return gpu_data_.light_view_projections; }
  std::vector<AllocatedImage>& get_images() { return gpu_data_.images_; }
  std::vector<VkSampler>& get_samplers() { return gpu_data_.samplers_; }

  void set_matrix(const size_t matrix, const glm::mat4& value) {
       gpu_data_.matrices_[matrix] = value;
  }
  void set_light_view_proj(const size_t matrix, const glm::mat4& value) {
       gpu_data_.light_view_projections[matrix] = value;
  }

  size_t get_vertices_size() const { return gpu_data_.vertices_.size(); }
  size_t get_indices_size() const { return gpu_data_.indices_.size(); }
  size_t get_surfaces_size() const { return gpu_data_.surfaces_.size(); }
  size_t get_matrices_size() const { return gpu_data_.matrices_.size(); }
  size_t get_images_size() const { return gpu_data_.images_.size(); }
  size_t get_samplers_size() const { return gpu_data_.samplers_.size(); }

  mesh_surface& get_surface(const size_t surface) { return gpu_data_.surfaces_[surface]; }
  glm::mat4& get_matrix(const size_t matrix) { return gpu_data_.matrices_[matrix]; }
  glm::mat4& get_light_view_proj(const size_t matrix) { return gpu_data_.light_view_projections[matrix]; }
  AllocatedImage& get_image(const size_t image) { return gpu_data_.images_[image]; }
  VkSampler& get_sampler(const size_t sampler) { return gpu_data_.samplers_[sampler]; }

  size_t add_mesh(const mesh_component& mesh) {
    components_.meshes_.push_back(mesh);
    return components_.meshes_.size() - 1;
  }
  size_t add_camera(const camera_component& camera) {
    components_.cameras_.push_back(camera);
    return components_.cameras_.size() - 1;
  }
  size_t add_light(const light_component& light) {
    components_.lights_.push_back(light);
    return components_.lights_.size() - 1;
  }
  size_t add_material(const material_component& material) {
    components_.materials_.push_back(material);
    return components_.materials_.size() - 1;
  }
  size_t add_transform(const transform_component& transform) {
    components_.transforms_.push_back(transform);
    return components_.transforms_.size() - 1;
  }

  std::vector<mesh_component>& get_meshes() { return components_.meshes_; }
  std::vector<camera_component>& get_cameras() { return components_.cameras_; }
  std::vector<std::reference_wrapper<light_component>> get_lights(light_type type) { 
      std::vector<std::reference_wrapper<light_component>> lights;
      for (auto& light : components_.lights_) {
        if (light.type == type) {
          lights.push_back(std::ref(light));
        }
      }
      return lights;
  }
  size_t max_lights(light_type type) {
      if (type == light_type::directional) {
        return 2;
      } else if (type == light_type::point) {
        return 256;
      } else if (type == light_type::spot) {
        return 0; //TDOO: Implement
      } else {
        return 0;
      }
  }
  std::vector<material_component>& get_materials() { return components_.materials_; }
  std::vector<transform_component>& get_transforms() { return components_.transforms_; }

  size_t get_meshes_size() const { return components_.meshes_.size(); }
  size_t get_cameras_size() const { return components_.cameras_.size(); }
  size_t get_lights_size() const { return components_.lights_.size(); }
  size_t get_materials_size() const { return components_.materials_.size(); }
  size_t get_transforms_size() const { return components_.transforms_.size(); }

  mesh_component& get_mesh(const size_t mesh) { return components_.meshes_[mesh]; }
  camera_component& get_camera(const size_t camera) { return components_.cameras_[camera]; }
  light_component& get_light(const size_t light) { return components_.lights_[light]; }
  material_component& get_material(const size_t material) {
    return components_.materials_[material];
  }
  transform_component& get_transform(const size_t transform) {
    return components_.transforms_[transform];
  }

  struct default_material {
    AllocatedImage color_image;
    AllocatedImage metallic_roughness_image;
    AllocatedImage normal_image;
    AllocatedImage emissive_image;
    AllocatedImage occlusion_image;

    AllocatedImage error_checkerboard_image;

    VkSampler default_sampler_linear;
    VkSampler default_sampler_nearest;
  } default_material_ = {};

private:
  struct gpu_data_container {
    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;
    std::vector<mesh_surface> surfaces_;
    std::vector<glm::mat4> matrices_;
    std::vector<glm::mat4> light_view_projections;
    std::vector<AllocatedImage> images_;
    std::vector<VkSampler> samplers_;
  } gpu_data_ = {};

  struct component_container {
    mesh_container meshes_;
    camera_container cameras_;
    light_container lights_;
    material_container materials_;
    transform_container transforms_;
  } components_ = {};
};
