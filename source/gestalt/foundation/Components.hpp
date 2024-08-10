#pragma once
#include <variant>
#include <vector>
#include "vk_types.hpp"
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include "GpuResources.hpp"


namespace gestalt::foundation {

    using Entity = uint32;
    constexpr Entity root_entity = 0;
    constexpr uint32 invalid_entity = std::numeric_limits<uint32>::max();
    constexpr size_t no_component = std::numeric_limits<size_t>::max();
    constexpr size_t default_material = 0;

    struct alignas(16) Meshlet {
      glm::vec3 center;
      float32 radius;

      int8 cone_axis[3];
      int8 cone_cutoff;

      uint32 vertex_offset; // offset into the global vertex buffer for this meshlet
      uint32 index_offset; // offset into the global index buffer for this meshlet
      uint32 mesh_draw_index;  // index into the mesh or MeshDraw that this meshlet belongs to
      uint8 vertex_count;
      uint8 triangle_count;
    };

  static_assert(sizeof(Meshlet) % 16 == 0);

  /**
   * \brief A MeshSurface is a single renderable part of a Mesh. It references the geometry and the material for a single
   * Mesh. The MeshSurface is rendered as a single draw call, and the geometry is split into Meshlets.
   */
  struct MeshSurface {
      uint32 meshlet_offset;
      uint32 meshlet_count;

      uint32 vertex_count;
      uint32 index_count;
      uint32 first_index;
      uint32 vertex_offset;
      BoundingSphere local_bounds;

      size_t material = default_material;
      size_t mesh_draw = no_component;
    };

    struct Component {
      mutable bool is_dirty = true;
    };

    struct MeshComponent : Component {
      size_t mesh;
    };

    /**
     * \brief A Mesh holds all the data for a single renderable "object" in the scene. It has one or more MeshSurfaces
     * that are rendered together. The Mesh function like a node in the scene graph, because moving the mesh moves all
     * MeshSurfaces together. A MeshSurface references the geometry and the material for a single renderable part of the Mesh.
     */
    struct Mesh {
      std::string name;
      std::vector<MeshSurface> surfaces;
      BoundingSphere local_bounds;
    };

  enum CameraType { kPerspective, kOrthographic };
  enum CameraPositionerType { kFreeFly, kOrbit };

    struct CameraComponent : Component {

      explicit CameraComponent(CameraType type, CameraPositionerType positioner)
        : type(type), positioner(positioner) {
      }

      CameraType type;
      CameraPositionerType positioner;
      glm::mat4 view_matrix{1.0f};
      glm::mat4 projection_matrix{1.0};
    };

    enum class LightType { kDirectional, kPoint, kSpot };

    struct LightBase {
        glm::vec3 color;
        float32 intensity;
      };

    struct DirectionalLightData {
        uint32 light_view_projection;
      };

      struct PointLightData {
        float32 range;
        uint32 first_light_view_projection; // index into the light view projection buffer the first out of 6 faces of the cube map
      };

      struct SpotLightData {
        float32 inner_cone;
        float32 outer_cone;
      };

      using LightSpecificData = std::variant<DirectionalLightData, PointLightData, SpotLightData>;

    struct LightComponent : Component {
        LightType type;
        LightBase base;
        LightSpecificData specific;

        LightComponent(glm::vec3 color, float32 intensity, uint32 light_view_projection)
            : type(LightType::kDirectional),
              base{color, intensity},
              specific(DirectionalLightData{light_view_projection}) {}

        LightComponent(glm::vec3 color, float32 intensity, float32 range,
                       uint32 first_light_view_projection)
            : type(LightType::kPoint), base{color, intensity}, specific(PointLightData{range, first_light_view_projection}) {}

        LightComponent(glm::vec3 color, float32 intensity, float32 inner_cone, float32 outer_cone)
            : type(LightType::kSpot),
              base{color, intensity},
              specific(SpotLightData{inner_cone, outer_cone}) {}

    };

    constexpr auto kUnusedTexture = std::numeric_limits<uint16>::max();
    constexpr uint32 kAlbedoTextureFlag = 0x01;
    constexpr uint32 kMetalRoughTextureFlag = 0x02;
    constexpr uint32 kNormalTextureFlag = 0x04;
    constexpr uint32 kEmissiveTextureFlag = 0x08;
    constexpr uint32 kOcclusionTextureFlag = 0x10;

    struct PbrMaterial {
      bool double_sided{false};
      bool transparent{false};

      struct alignas(64) PbrConstants {
        uint16 albedo_tex_index = kUnusedTexture;
        uint16 metal_rough_tex_index = kUnusedTexture;
        uint16 normal_tex_index = kUnusedTexture;
        uint16 emissive_tex_index = kUnusedTexture;
        uint16 occlusion_tex_index = kUnusedTexture;
        uint32 flags{0};

        glm::vec4 albedo_color{1.f};
        glm::vec2 metal_rough_factor = {.0f, 0.f};  // roughness, metallic
        float32 occlusionStrength{1.f};
        float32 alpha_cutoff{0.f};
        glm::vec3 emissiveColor{0.f};
        float32 emissiveStrength{1.0};
      } constants;

      struct PbrTextures {
        TextureHandle albedo_image;
        VkSampler albedo_sampler;
        TextureHandle metal_rough_image;
        VkSampler metal_rough_sampler;
        TextureHandle normal_image;
        VkSampler normal_sampler;
        TextureHandle emissive_image;
        VkSampler emissive_sampler;
        TextureHandle occlusion_image;
        VkSampler occlusion_sampler;
      } textures;
    };

    struct Material {
      std::string name;
      PbrMaterial config;
      bool is_dirty = true;
    };

    struct TransformComponent : Component {
      glm::vec3 position;
      glm::quat rotation;
      float32 scale;  // uniform scale for now

      glm::vec3 parent_position;
      glm::quat parent_rotation;
      float32 parent_scale;

      TransformComponent operator*(const TransformComponent& local_transform) const {
        TransformComponent result;

        // Compute the combined world position
        result.position = position + rotation * (scale * local_transform.position);

        // Compute the combined world rotation
        result.rotation = rotation * local_transform.rotation;

        // Compute the combined world scale
        result.scale = scale * local_transform.scale;

        return result;
      }
    };

    struct NodeComponent :Component {
      std::string name;
      Entity parent = invalid_entity;
      std::vector<Entity> children;
      AABB bounds;
      bool visible = true;
    };

}  // namespace gestalt