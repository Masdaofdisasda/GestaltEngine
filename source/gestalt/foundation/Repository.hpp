#pragma once

#include <optional>
#include <unordered_map>

#include "GpuResources.hpp"
#include "Components/CameraComponent.hpp"
#include "Components/Entity.hpp"
#include "Components/LightComponent.hpp"
#include "Components/MeshComponent.hpp"
#include "Components/NodeComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "Material/Material.hpp"
#include "Mesh/Mesh.hpp"
#include "Mesh/Meshlet.hpp"

namespace gestalt::foundation {

  template <typename ComponentType> class ComponentContainer {
  public:
    size_t size() const { return components_.size(); }

    std::optional<std::reference_wrapper<ComponentType>> get(const Entity& ent) {
      auto it = components_.find(ent);
      if (it != components_.end()) {
        return std::ref(it->second);
      }
      return std::nullopt;
    }

    std::unordered_map<Entity, ComponentType>& components() { return components_; }

    std::vector<std::pair<Entity, std::reference_wrapper<ComponentType>>> asVector() {
      std::vector<std::pair<Entity, std::reference_wrapper<ComponentType>>> result;
      for (auto& [ent, comp] : components_) {
        result.emplace_back(ent, std::ref(comp));
      }
      return result;
    }

    void add(const Entity& ent, const ComponentType& component) {
      components_.insert({ent, component});
    }

    void remove(const Entity& ent) { components_.erase(ent); }

    void clear() { components_.clear(); }

  private:
    std::unordered_map<Entity, ComponentType> components_;
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

    void set(const size_t index, const DataType& value) { data_[index] = value; }

    void remove(const size_t index) { data_.erase(data_.begin() + index); }

    void clear() { data_.clear(); }

  private:
    std::vector<DataType> data_;
  };

  class Repository : public NonCopyable<Repository> {

  public:

    struct default_material {
      TextureHandle color_image;
      VkSampler color_sampler;
      TextureHandle metallic_roughness_image;
       VkSampler metallic_roughness_sampler;
      TextureHandle normal_image;
      VkSampler normal_sampler;
      TextureHandle emissive_image;
      VkSampler emissive_sampler;
      TextureHandle occlusion_image;
      VkSampler occlusion_sampler;

      TextureHandle error_checkerboard_image;
    } default_material_ = {};

    std::unique_ptr<MaterialBuffers> material_buffers = std::make_unique<MaterialBuffers>();
    std::unique_ptr<MeshBuffers> mesh_buffers = std::make_unique<MeshBuffers>();
    std::unique_ptr<LightBuffers> light_buffers = std::make_unique<LightBuffers>();
    std::unique_ptr<PerFrameDataBuffers> per_frame_data_buffers = std::make_unique<PerFrameDataBuffers>();

    GpuDataContainer<GpuVertexPosition> vertex_positions;
    GpuDataContainer<GpuVertexData> vertex_data;
    GpuDataContainer<uint32> indices;
    GpuDataContainer<Meshlet> meshlets;
    GpuDataContainer<uint32> meshlet_vertices;
    GpuDataContainer<uint8> meshlet_triangles;
    GpuDataContainer<glm::mat4> light_view_projections;
    GpuDataContainer<GpuDirectionalLight> directional_lights;
    GpuDataContainer<GpuPointLight> point_lights;
    GpuDataContainer<TextureHandle> textures;
    GpuDataContainer<Material> materials;
    GpuDataContainer<Mesh> meshes;
    GpuDataContainer<MeshDraw> mesh_draws;
    std::unordered_map<SamplerConfig, VkSampler, SamplerConfigHash> sampler_cache;

    VkSampler get_sampler(const SamplerConfig& config = {}) {
      if (const auto it = sampler_cache.find(config); it != sampler_cache.end()) {
        return it->second;
      }
      return nullptr;
    }

    ComponentContainer<NodeComponent> scene_graph;
    ComponentContainer<MeshComponent> mesh_components;
    ComponentContainer<CameraComponent> camera_components;
    ComponentContainer<LightComponent> light_components;
    ComponentContainer<TransformComponent> transform_components;
  };
}  // namespace gestalt::foundation