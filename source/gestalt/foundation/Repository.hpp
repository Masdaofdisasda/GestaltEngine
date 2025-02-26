#pragma once

#include <memory>
#include <optional>
#include <unordered_map>

#include "Buffer/LightBuffer.hpp"
#include "Buffer/MaterialBuffer.hpp"
#include "Buffer/MeshBuffer.hpp"
#include "Buffer/PerFrameDataBuffer.hpp"
#include "Buffer/RayTracingBuffer.hpp"
#include "Components/AnimationComponent.hpp"
#include "Components/CameraComponent.hpp"
#include "Components/Entity.hpp"
#include "Components/LightComponent.hpp"
#include "Components/MeshComponent.hpp"
#include "Components/NodeComponent.hpp"
#include "Components/TransformComponent.hpp"
#include "Components/PhysicsComponent.hpp"
#include "Material/Material.hpp"
#include "Mesh/Mesh.hpp"
#include "Mesh/MeshDraw.hpp"
#include "Mesh/Meshlet.hpp"
#include "Resources/GpuDirectionalLight.hpp"
#include "Resources/GpuPointLight.hpp"
#include "Resources/GpuProjViewData.hpp"
#include "Resources/GpuSpotLight.hpp"
#include "Resources/GpuVertexData.hpp"
#include "Resources/GpuVertexPosition.hpp"

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

    // NOTE: this does not check if the entity exists
    ComponentType& operator[](const Entity& ent) {
      auto it = components_.find(ent);
      if (it != components_.end()) {
        return it->second;
      }
      throw std::runtime_error("Component not found");
    }

    // NOTE: this does not check if the entity exists
    const ComponentType& operator[](const Entity& ent) const {
      auto it = components_.find(ent);
      if (it != components_.end()) {
        return it->second;
      }
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
      std::shared_ptr<ImageInstance> color_image_instance;
      std::unique_ptr<SamplerInstance> color_sampler;

      std::shared_ptr<ImageInstance> metallic_roughness_image_instance;
      std::unique_ptr<SamplerInstance> metallic_roughness_sampler;

      std::shared_ptr<ImageInstance> normal_image_instance;
      std::unique_ptr<SamplerInstance> normal_sampler;

      std::shared_ptr<ImageInstance> emissive_image_instance;
      std::unique_ptr<SamplerInstance> emissive_sampler;

      std::shared_ptr<ImageInstance> occlusion_image_instance;
      std::unique_ptr<SamplerInstance> occlusion_sampler;

      std::shared_ptr<ImageInstance> error_checkerboard_image_instance;

    } default_material_ = {};

    std::unique_ptr<MaterialBuffers> material_buffers = std::make_unique<MaterialBuffers>();
    std::unique_ptr<MeshBuffers> mesh_buffers = std::make_unique<MeshBuffers>();
    std::unique_ptr<LightBuffers> light_buffers = std::make_unique<LightBuffers>();
    std::unique_ptr<PerFrameDataBuffers> per_frame_data_buffers = std::make_unique<PerFrameDataBuffers>();
    std::unique_ptr<RayTracingBuffer> ray_tracing_buffer = std::make_unique<RayTracingBuffer>();

    std::shared_ptr<AccelerationStructure> tlas;
    std::shared_ptr<BufferInstance>        tlas_instance_buffer;
    std::shared_ptr<BufferInstance>        tlas_storage_buffer;

    GpuDataContainer<GpuVertexPosition> vertex_positions;
    GpuDataContainer<GpuVertexData> vertex_data;
    GpuDataContainer<uint32> indices;
    GpuDataContainer<Meshlet> meshlets;
    GpuDataContainer<uint32> meshlet_vertices;
    GpuDataContainer<uint8> meshlet_triangles;
    GpuDataContainer<GpuProjViewData> light_view_projections;
    GpuDataContainer<GpuDirectionalLight> directional_lights;
    GpuDataContainer<GpuPointLight> point_lights;
    GpuDataContainer<GpuSpotLight> spot_lights;
    GpuDataContainer<std::shared_ptr<ImageInstance>> textures;
    GpuDataContainer<Material> materials;
    GpuDataContainer<Mesh> meshes;
    GpuDataContainer<MeshDraw> mesh_draws;

    std::vector<MeshDraw> mesh_draws_; ///actual one, super cursed i know

    ComponentContainer<NodeComponent> scene_graph;
    ComponentContainer<MeshComponent> mesh_components;
    ComponentContainer<CameraComponent> camera_components;
    ComponentContainer<LightComponent> light_components;
    ComponentContainer<AnimationComponent> animation_components;
    ComponentContainer<TransformComponent> transform_components;
    ComponentContainer<PhysicsComponent> physics_components;
  };
}  // namespace gestalt::foundation