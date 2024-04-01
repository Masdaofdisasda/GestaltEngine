#pragma once

#include "vk_types.h"
#include "Components.h"

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

struct EngineLimits {
  size_t directional_lights = 2; // actually only 1 is used, but we need 2 to fulfill the 64 bytes alignment
  size_t point_lights = 256;
  size_t spot_lights = 0; // not implemented yet
  size_t materials = -1; //TODO
  size_t max_textures = -1;
  size_t max_samplers = -1;
  size_t max_cameras = -1;
} constexpr kLimits = {};

class Repository {
public:

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
  GpuDataContainer<Material> materials;
  GpuDataContainer<Mesh> meshes;
  GpuDataContainer<CameraData> cameras;

  ComponentContainer<NodeComponent> scene_graph;
  ComponentContainer<MeshComponent> mesh_components;
  ComponentContainer<CameraComponent> camera_components;
  ComponentContainer<LightComponent> light_components;
  ComponentContainer<TransformComponent> transform_components;
};
