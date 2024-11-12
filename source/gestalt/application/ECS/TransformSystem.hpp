#pragma once

#include "Repository.hpp"
#include "BaseSystem.hpp"

namespace gestalt::application {

    class TransformSystem final : public BaseSystem, public NonCopyable<TransformSystem> {
      void mark_children_bounds_dirty(Entity entity);
      void mark_bounds_as_dirty(Entity entity);
      void update_aabb(Entity entity, const TransformComponent& parent_transform);
      void mark_parent_bounds_dirty(Entity entity);

    public:
      static glm::mat4 get_model_matrix(const TransformComponent& transform);

      void update(float delta_time, const UserInput& movement, float aspect) override;
    };

}  // namespace gestalt