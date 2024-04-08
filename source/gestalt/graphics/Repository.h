#pragma once

#include <typeindex>

#include "vk_types.h"
#include "Components.h"
#include "GpuResources.h"

namespace gestalt {
  namespace foundation {

    class DataHolder {
    public:
      virtual ~DataHolder() = default;
    };

    template <typename T> class Holder : public DataHolder {
    public:
      T data;

      // Constructor to initialize data if needed
      Holder(const T& initialData) : data(initialData) {}
      virtual ~Holder() = default;
    };


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

      std::vector<std::pair<entity, std::reference_wrapper<ComponentType>>> asVector() {
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

      void set(const size_t index, const DataType& value) { data_[index] = value; }

      void remove(const size_t index) { data_.erase(data_.begin() + index); }

      void clear() { data_.clear(); }

    private:
      std::vector<DataType> data_;
    };

    struct EngineLimits {
      size_t max_directional_lights
          = 2;  // actually only 1 is used, but we need 2 to fulfill the 64 bytes alignment
      size_t max_point_lights = 256;
      size_t max_spot_lights = 0;  // not implemented yet
      size_t max_materials = 256;   // each material can have up to 5 textures (PBR)
      size_t max_textures = max_materials * 5;
      size_t max_cameras = -1;
    } constexpr kLimits = {};

    class Repository {

      
    std::unordered_map<std::type_index, std::unique_ptr<DataHolder>> holders;

    public:

      template <typename T> void add_buffer(const T& data) {
        const auto typeIndex = std::type_index(typeid(T));
        assert(holders.find(typeIndex) == holders.end()
               && "Instance for this type already exists.");
        holders[typeIndex] = std::make_unique<Holder<T>>(data);
      }

      template <typename T> T& get_buffer() {
        const auto typeIndex = std::type_index(typeid(T));
        assert(holders.find(typeIndex) != holders.end()
               && "Instance for this type does not exist.");
        auto holder = dynamic_cast<Holder<T>*>(holders[typeIndex].get());
        assert(holder != nullptr && "Holder cast failed.");
        return holder->data;
      }

      struct default_material {
        TextureHandle color_image;
        TextureHandle metallic_roughness_image;
        TextureHandle normal_image;
        TextureHandle emissive_image;
        TextureHandle occlusion_image;

        TextureHandle error_checkerboard_image;

        VkSampler linearSampler;
        VkSampler nearestSampler;
      } default_material_ = {};

      MaterialData material_data;

      DrawContext main_draw_context_;  // TODO replace with draw command buffer

      GpuDataContainer<GpuVertexPosition> vertex_positions;
      GpuDataContainer<GpuVertexData> vertex_data;
      GpuDataContainer<uint32_t> indices;
      GpuDataContainer<glm::mat4> model_matrices;
      GpuDataContainer<glm::mat4> light_view_projections;
      GpuDataContainer<GpuDirectionalLight> directional_lights;
      GpuDataContainer<GpuPointLight> point_lights;
      GpuDataContainer<TextureHandle> textures;
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
  }  // namespace foundation
}  // namespace gestalt