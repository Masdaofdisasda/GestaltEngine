#pragma once

#include "vk_types.h"
#include "scene_components.h"

// Component storage types
template <typename component_type> using component_container
    = std::unordered_map<entity, component_type>;

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
  size_t add_material(const material& material) {
    gpu_data_.materials_.push_back(material);
    return gpu_data_.materials_.size() - 1;
  }
  size_t add_mesh(const mesh& mesh) {
    gpu_data_.meshes_.push_back(mesh);
    return gpu_data_.meshes_.size() - 1;
  }
  size_t add_camera(const camera_data& camera) {
       gpu_data_.cameras_.push_back(camera);
    return gpu_data_.cameras_.size() - 1;
  }

  std::vector<Vertex>& get_vertices() { return gpu_data_.vertices_; }
  std::vector<uint32_t>& get_indices() { return gpu_data_.indices_; }
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
  size_t get_matrices_size() const { return gpu_data_.matrices_.size(); }
  size_t get_images_size() const { return gpu_data_.images_.size(); }
  size_t get_samplers_size() const { return gpu_data_.samplers_.size(); }
  size_t get_materials_size() const { return gpu_data_.materials_.size(); }
  size_t get_meshes_size() const { return gpu_data_.meshes_.size(); }
  size_t get_light_view_projs_size() const { return gpu_data_.light_view_projections.size(); }

  glm::mat4& get_matrix(const size_t matrix) { return gpu_data_.matrices_[matrix]; }
  glm::mat4& get_light_view_proj(const size_t matrix) { return gpu_data_.light_view_projections[matrix]; }
  AllocatedImage& get_image(const size_t image) { return gpu_data_.images_[image]; }
  VkSampler& get_sampler(const size_t sampler) { return gpu_data_.samplers_[sampler]; }
  mesh& get_mesh(const size_t mesh) { return gpu_data_.meshes_[mesh]; }
  material& get_material(const size_t material) { return gpu_data_.materials_[material]; }
  camera_data& get_camera(const size_t camera) { return gpu_data_.cameras_[camera]; }

  void add_node(const entity entity, const node_component& node) {
       components_.scene_graph_.insert({entity, node});
  }
  void add_mesh(const entity entity, const mesh_component& mesh) {
    components_.meshes_.insert({entity, mesh});
  }
  void add_camera(const entity entity, const camera_component& camera) {
    components_.cameras_.insert({entity, camera});
  }
  void add_light(const entity entity, const light_component& light) {
    components_.lights_.insert({entity, light});
  }
  void add_transform(const entity entity, const transform_component& transform) {
    components_.transforms_.insert({entity, transform});
  }

  component_container<node_component>& get_scene_graph() { return components_.scene_graph_; }
  component_container<mesh_component>& get_meshes() { return components_.meshes_; }
  component_container<camera_component>& get_cameras() { return components_.cameras_; }
  component_container<light_component>& get_lights() { return components_.lights_; }
  std::vector<std::pair<entity, light_component&>> get_lights(light_type type) {
    std::vector<std::pair<entity, light_component&>> lights;
    for (auto& [entity, light] : components_.lights_) {
      if (light.type == type) {
        lights.emplace_back(entity, light);
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
  component_container<transform_component>& get_transforms() { return components_.transforms_; }

  size_t get_mesh_components_size() const { return components_.meshes_.size(); }
  size_t get_camera_components_size() const { return components_.cameras_.size(); }
  size_t get_light_components_size() const { return components_.lights_.size(); }
  size_t get_transform_components_size() const { return components_.transforms_.size(); }

  std::optional<std::reference_wrapper<mesh_component>> get_mesh_component(const size_t mesh) {
      auto it = components_.meshes_.find(mesh);
      if (it != components_.meshes_.end()) {
        return std::ref(it->second);
      } else {
        return std::nullopt;
      }
  }

  std::optional<std::reference_wrapper<node_component>> get_node_component(const entity entity) {
      auto it = components_.scene_graph_.find(entity);
      if (it != components_.scene_graph_.end()) {
        return std::ref(it->second);
      } else {
        return std::nullopt;
      }
  }

  std::optional<std::reference_wrapper<camera_component>> get_camera_component(
      const entity entity) {
      auto it = components_.cameras_.find(entity);
      if (it != components_.cameras_.end()) {
        return std::ref(it->second);
      } else {
        return std::nullopt;
      }
  }

  std::optional<std::reference_wrapper<light_component>> get_light_component(const entity entity) {
      auto it = components_.lights_.find(entity);
      if (it != components_.lights_.end()) {
        return std::ref(it->second);
      } else {
        return std::nullopt;
      }
  }

  std::optional<std::reference_wrapper<transform_component>> get_transform_component(
      const entity entity) {
      auto it = components_.transforms_.find(entity);
      if (it != components_.transforms_.end()) {
        return std::ref(it->second);
      } else {
        return std::nullopt;
      }
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
    std::vector<glm::mat4> matrices_;
    std::vector<glm::mat4> light_view_projections;
    std::vector<AllocatedImage> images_;
    std::vector<VkSampler> samplers_;
    std::vector<material> materials_;
    std::vector<mesh> meshes_;
    std::vector<camera_data> cameras_;
  } gpu_data_ = {};

  struct component_containers {
    component_container<node_component> scene_graph_;
    component_container<mesh_component> meshes_;
    component_container<camera_component> cameras_;
    component_container<light_component> lights_;
    component_container<transform_component> transforms_;
  } components_ = {};
};
