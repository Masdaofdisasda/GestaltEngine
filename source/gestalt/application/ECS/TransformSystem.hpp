#pragma once

#include "Repository.hpp"
#include "BaseSystem.hpp"

namespace gestalt::application {

    class TransformSystem final {
    Repository& repository_;

      void mark_children_bounds_dirty(Entity entity);
      void mark_bounds_as_dirty(Entity entity);
      void update_aabb(Entity entity, const TransformComponent& parent_transform);
      void mark_parent_bounds_dirty(Entity entity);

    public:
    explicit TransformSystem(Repository& repository);
      ~TransformSystem() = default;

      TransformSystem(const TransformSystem&) = delete;
      TransformSystem& operator=(const TransformSystem&) = delete;

      TransformSystem(TransformSystem&&) = delete;
      TransformSystem& operator=(TransformSystem&&) = delete;

      static glm::mat4 get_model_matrix(const TransformComponent& transform);

      void update();
    };

}  // namespace gestalt