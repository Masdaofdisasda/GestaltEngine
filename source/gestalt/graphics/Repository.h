#pragma once

#include "vk_types.h"
#include "scene_components.h"

template <typename ComponentType> class ComponentContainer {
public:

  size_t size() const { return components_.size(); }

  std::optional<std::reference_wrapper<ComponentType>> get(const entity& ent) {
    auto it = components_.find(ent);
    if (it != components_.end()) {
      return std::ref(it->second);
    }
    return std::nullopt;
  }

  std::unordered_map<entity, ComponentType>& components() { return components_; }

  std::vector<std::pair<entity, std::reference_wrapper<ComponentType>>>
  asVector() {
    std::vector<std::pair<entity, std::reference_wrapper<ComponentType>>> result;
    for (auto& [ent, comp] : components_) {
      result.emplace_back(ent, std::ref(comp));
    }
    return result;
  }

  void add(const entity& ent, const ComponentType& component) {
    components_.insert({ent, component});
  }

  void remove(const entity& ent) { components_.erase(ent); }

private:
  std::unordered_map<entity, ComponentType> components_;
};

template <typename DataType> class GpuDataContainer {
public:

  size_t size() const { return data_.size(); }

  std::vector<DataType>& data() { return data_; }

  size_t add(const std::vector<DataType>& newData) {
    const size_t offset = data_.size();
    data_.insert(data_.end(), newData.begin(), newData.end());
    return offset;
  }

  size_t add(const DataType& newData) {
    const size_t offset = data_.size();
    data_.push_back(newData);
    return offset;
  }

  DataType& get(const size_t index) { return data_[index]; }

  void set(const size_t index, const DataType& value) { data_[index] = value;}

  void remove (const size_t index) { data_.erase(data_.begin() + index); }

private:
  std::vector<DataType> data_;
};

class Repository {
public:


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

  struct default_material {
    AllocatedImage color_image;
    AllocatedImage metallic_roughness_image;
    AllocatedImage normal_image;
    AllocatedImage emissive_image;
    AllocatedImage occlusion_image;

    AllocatedImage error_checkerboard_image;

    VkSampler linearSampler;
    VkSampler nearestSampler;
  } default_material_ = {};

  GpuDataContainer<GpuVertexPosition> vertex_positions;
  GpuDataContainer<GpuVertexData> vertex_data;
  GpuDataContainer<uint32_t> indices;
  GpuDataContainer<glm::mat4> model_matrices;
  GpuDataContainer<glm::mat4> light_view_projections;
  GpuDataContainer<AllocatedImage> textures;
  GpuDataContainer<VkSampler> samplers;
  GpuDataContainer<material> materials;
  GpuDataContainer<mesh> meshes;
  GpuDataContainer<camera_data> cameras;

  ComponentContainer<node_component> scene_graph;
  ComponentContainer<mesh_component> mesh_components;
  ComponentContainer<camera_component> camera_components;
  ComponentContainer<light_component> light_components;
  ComponentContainer<transform_component> transform_components;
};
