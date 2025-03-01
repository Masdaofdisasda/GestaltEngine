#pragma once

#include "Repository.hpp"

namespace gestalt::foundation {
  struct FrameProvider;
  class IResourceAllocator;
}

namespace gestalt::application {

    class LightSystem final {
      IGpu& gpu_;
      IResourceAllocator& resource_allocator_;
      Repository& repository_;
      FrameProvider& frame_;

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
      LightSystem(IGpu& gpu, IResourceAllocator& resource_allocator, Repository& repository,
                  FrameProvider& frame);
      ~LightSystem();

      LightSystem(const LightSystem&) = delete;
      LightSystem& operator=(const LightSystem&) = delete;

      LightSystem(LightSystem&&) = delete;
      LightSystem& operator=(LightSystem&&) = delete;

      void update();
    };

}  // namespace gestalt