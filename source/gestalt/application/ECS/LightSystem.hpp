#pragma once

#include "Repository.hpp"
#include "BaseSystem.hpp"

namespace gestalt::application {

    class LightSystem final : public BaseSystem, public NonCopyable<LightSystem> {

      void create_buffers();

      glm::mat4 calculate_directional_light_view_matrix(glm::vec3 direction) const;

      glm::mat4 calculate_directional_light_proj_matrix(const glm::mat4& light_view,
                                                        glm::mat4 inv_cam) const;

    public:
      void prepare() override;
      void update(float delta_time, const UserInput& movement, float aspect) override;
      void cleanup() override;
    };

}  // namespace gestalt