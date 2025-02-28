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

  template <typename ComponentType> class ComponentStorage {
  public:
    [[nodiscard]] const ComponentType* find(Entity ent) const {
      auto it = components_.find(ent);
      return (it != components_.end()) ? &it->second : nullptr;
    }

    [[nodiscard]] ComponentType* find_mutable(Entity ent) {
      auto it = components_.find(ent);
      return (it != components_.end()) ? &it->second : nullptr;
    }

    void upsert(Entity ent, const ComponentType& component) {
      components_.insert_or_assign(ent, component);
    }

    void remove(Entity ent) { components_.erase(ent); }

    [[nodiscard]] std::vector<std::pair<Entity, ComponentType>> snapshot() const {
      std::vector<std::pair<Entity, ComponentType>> result;
      result.reserve(components_.size());
      for (auto& [ent, comp] : components_) {
        result.emplace_back(ent, comp);  // copy
      }
      return result;
    }

    [[nodiscard]] size_t size() const { return components_.size(); }

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

  class Repository final {

  public:
    Repository() = default;
    ~Repository() = default;

    Repository(const Repository&) = delete;
    Repository& operator=(const Repository&) = delete;

    Repository(Repository&&) = delete;
    Repository& operator=(Repository&&) = delete;

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

    ComponentStorage<NodeComponent> scene_graph;
    ComponentStorage<MeshComponent> mesh_components;
    ComponentStorage<CameraComponent> camera_components;
    ComponentStorage<LightComponent> light_components;
    ComponentStorage<AnimationComponent> animation_components;
    ComponentStorage<TransformComponent> transform_components;
    ComponentStorage<PhysicsComponent> physics_components;
  };
}  // namespace gestalt::foundation