#pragma once

#include "Repository.hpp"
#include "BaseSystem.hpp"

namespace gestalt::application {

    class LightSystem final : public BaseSystem {

      void create_buffers();

      [[nodiscard]] glm::mat4 calculate_directional_light_view_matrix(glm::vec3 direction) const;

      glm::mat4 calculate_directional_light_view_matrix_cam_only(const glm::vec3 direction,
                                                                 glm::mat4 cam_inv) const;

      [[nodiscard]] glm::mat4 calculate_directional_light_proj_matrix_scene_only(
          const glm::mat4& light_view) const;

      static glm::mat4 calculate_directional_light_proj_matrix_camera_only(const glm::mat4& light_view,
        const glm::mat4& inv_cam,
        float ndc_min_z, float shadowMapResolution, float zMult);

    public:
      LightSystem() = default;
      ~LightSystem() override = default;

      LightSystem(const LightSystem&) = delete;
      LightSystem& operator=(const LightSystem&) = delete;

      LightSystem(LightSystem&&) = delete;
      LightSystem& operator=(LightSystem&&) = delete;

      void prepare() override;
      void update(float delta_time, const UserInput& movement, float aspect) override;
      void cleanup() override;
    };

}  // namespace gestalt