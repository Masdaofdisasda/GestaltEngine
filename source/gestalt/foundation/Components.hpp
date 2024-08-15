#pragma once
#include <variant>
#include <vector>
#include "vk_types.hpp"
#include <glm/gtx/euler_angles.hpp>
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

  enum CameraProjectionType { kPerspective, kOrthographic };
  enum CameraPositionerType { kFreeFly, kOrbit, kFirstPerson, kAnimation };

  struct FreeFlyCameraData {

    FreeFlyCameraData(const glm::vec3& pos, const glm::vec3& target, const glm::vec3& up) {
      set_position(pos);
      set_orientation(target, up);
      set_up_vector(up);
    }

    void set_up_vector(const glm::vec3& up) {
      this->up = up;
      const glm::mat4 view = get_view_matrix();
      const glm::vec3 dir = -glm::vec3(view[0][2], view[1][2], view[2][2]);
      camera_orientation = lookAtRH(camera_position, camera_position + dir, up);
    }

    void set_position(const glm::vec3& pos) {
      camera_position = pos;
    }

    void set_orientation(const glm::vec3& target, const glm::vec3& up) {
      camera_orientation = quat_cast(lookAtRH(camera_position, target, up));
    }

    [[nodiscard]] glm::mat4 get_view_matrix() const {
      const glm::mat4 t = translate(glm::mat4(1.0f), -camera_position);
      const glm::mat4 r = mat4_cast(camera_orientation);
      return r * t;
    }

    void reset_mouse_position(const glm::vec2& p) { mouse_pos = p; }

    glm::vec3 camera_position = glm::vec3(0.0f, 0.0f, 5.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 1.0f);
    glm::quat camera_orientation = glm::quat(glm::vec3(0.0f, 0.0f, 0.0f));
    glm::vec3 move_speed = glm::vec3(0.0f);
    glm::vec2 mouse_pos = glm::vec2(0);

    float mouse_speed = 4.5f;
    float acceleration = .01f;
    float damping = 15.f;
    float max_speed = .05f;
    float fast_coef = 5.f;
    float slow_coef = .001f;
  };

  struct OrbitCameraData {

    OrbitCameraData(const glm::vec3& target, const float32 distance, const float32 min_distance, const float32 max_distance)
        : min_distance(min_distance),
          max_distance(max_distance) {
      set_target(target);
      set_distance(distance);
    }

    void set_target(const glm::vec3& target) {
      this->target = target;
    }

    void set_distance(const float32 distance) {
      this->distance = glm::clamp(distance, min_distance, max_distance);
    }


    [[nodiscard]] glm::mat4 get_view_matrix() const {
      return lookAtRH(position, target, up);
    }


    glm::vec3 target;
    float32 distance;
    float32 min_distance;
    float32 max_distance;

    float32 yaw = 45.0f;
    float32 pitch = .3f;
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat orientation = glm::quat(glm::vec3(0.0f));
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    glm::vec2 mouse_pos = glm::vec2(0);

    // Configuration parameters
    float32 orbit_speed = 2.f;
    float32 zoom_speed = 0.1f;
    float32 pan_speed = 10.0f;
  };

  struct FirstPersonCameraData {

    FirstPersonCameraData(const glm::vec3& start_position, const glm::vec3& start_up) {
      set_position(start_position);
      set_orientation(start_position + glm::vec3(0.0f, 0.0f, -1.0f), start_up);
      set_up_vector(start_up);
    }

    void set_position(const glm::vec3& pos) {
      camera_position = pos;
    }

    void set_orientation(const glm::vec3& target, const glm::vec3& up) {
      camera_orientation = quat_cast(lookAtRH(camera_position, target, up));
    }

    void set_up_vector(const glm::vec3& up) {
      this->up = up;
      const glm::mat4 view = get_view_matrix();
      const glm::vec3 dir = -glm::vec3(view[0][2], view[1][2], view[2][2]);
      camera_orientation = lookAtRH(camera_position, camera_position + dir, up);
    }

    [[nodiscard]] glm::mat4 get_view_matrix() const {
      const glm::mat4 t = translate(glm::mat4(1.0f), -camera_position);
      const glm::mat4 r = mat4_cast(camera_orientation);
      return r * t;
    }

    glm::vec3 camera_position;
    glm::quat camera_orientation = glm::quat(glm::vec3(0.0f));
    glm::vec3 up;

    glm::vec2 mouse_pos = glm::vec2(0.0f);

    float32 mouse_speed = 2.5f;  // Adjust based on your desired sensitivity
    glm::vec3 move_speed = glm::vec3(0.0f);
  };

  struct AnimationCameraData {

    AnimationCameraData(const glm::vec3& pos, const glm::vec3& angles, const glm::vec3& desired_pos, const glm::vec3& desired_angles)
        : position_current(pos),
          position_desired(desired_pos),
          angles_current(angles),
          angles_desired(desired_angles) {}

    glm::mat4 get_view_matrix() const {
      const glm::vec3 a = glm::radians(angles_current);
      return translate(glm::yawPitchRoll(a.y, a.x, a.z), -position_current);
    }

    float32 damping_linear = 10000.0f;
    glm::vec3 damping_euler_angles = glm::vec3(5.0f, 5.0f, 5.0f);

    glm::vec3 position_current = glm::vec3(0.0f);
    glm::vec3 position_desired = glm::vec3(0.0f);

    /// pitch, pan, roll
    glm::vec3 angles_current = glm::vec3(0.0f);
    glm::vec3 angles_desired = glm::vec3(0.0f);
  };

  using CameraData = std::variant<FreeFlyCameraData, FirstPersonCameraData, OrbitCameraData, AnimationCameraData>;

    struct CameraComponent : Component {

      explicit CameraComponent(const CameraProjectionType projection, const CameraPositionerType positioner, const CameraData& camera_data)
          : projection(projection), positioner(positioner), camera_data(camera_data) {
      }

      CameraProjectionType projection;
      const CameraPositionerType positioner;
      glm::mat4 view_matrix{1.0f};
      glm::mat4 projection_matrix{1.0};

      CameraData camera_data;
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