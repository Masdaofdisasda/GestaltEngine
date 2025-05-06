#include "TransformSystem.hpp"

#include "Repository.hpp"
#include "Events/EventBus.hpp"
#include "Events/Events.hpp"

namespace gestalt::application {

  TransformSystem::TransformSystem(Repository& repository, EventBus& event_bus)
      : repository_(repository) {
    event_bus.subscribe<MoveEntityEvent>([this](const MoveEntityEvent& event) {
      auto transform = repository_.transform_components.find_mutable(event.entity());
      transform->set_position(event.new_position());
      transform->set_rotation(event.new_rotation());
      transform->set_scale(event.new_scale());
      transform->is_dirty = true;
    });
    event_bus.subscribe<TranslateEntityEvent>([this](const TranslateEntityEvent& event) {
      auto transform = repository_.transform_components.find_mutable(event.entity());
      transform->set_position(event.new_position());
      transform->is_dirty = true;
    });
    event_bus.subscribe<RotateEntityEvent>([this](const RotateEntityEvent& event) {
      auto transform = repository_.transform_components.find_mutable(event.entity());
      transform->set_rotation(event.new_rotation());
      transform->is_dirty = true;
    });
    event_bus.subscribe<ScaleEntityEvent>([this](const ScaleEntityEvent& event) {
      auto transform = repository_.transform_components.find_mutable(event.entity());
      transform->set_scale(event.new_scale());
      transform->is_dirty = true;
    });
  }

  glm::mat4 TransformSystem::get_model_matrix(const TransformComponent& transform) {
    return translate(glm::mat4(1.0f), transform.position()) * mat4_cast(transform.rotation())
           * scale(glm::mat4(1.0f), glm::vec3(transform.scale()));
  }

  void TransformSystem::mark_parent_bounds_dirty(Entity entity) {
    if (entity == invalid_entity) {
      return;
    }

    const auto node = repository_.scene_graph.find_mutable(entity);
    if (node->bounds.is_dirty) {
      return;
    }

    node->bounds.is_dirty = true;
    mark_parent_bounds_dirty(node->parent);
  }

  void TransformSystem::mark_children_bounds_dirty(Entity entity) {
    if (entity == invalid_entity) {
      return;
    }

    const auto node = repository_.scene_graph.find_mutable(entity);
    if (node->bounds.is_dirty) {
      return;
    }

    node->bounds.is_dirty = true;
    for (const auto& child : node->children) {
      mark_children_bounds_dirty(child);
    }
  }

  void TransformSystem::mark_bounds_as_dirty(Entity entity) {
    const auto node = repository_.scene_graph.find_mutable(entity);
    node->bounds.is_dirty = true;
    for (const auto& child : node->children) {
      mark_children_bounds_dirty(child);
    }
    mark_parent_bounds_dirty(node->parent);
  }

  void TransformSystem::update_aabb(const Entity entity,
                                    const TransformComponent& parent_transform) {
    auto node = repository_.scene_graph.find_mutable(entity);
    if (!node->bounds.is_dirty) {
      return;
    }
    auto local_transform = repository_.transform_components.find_mutable(entity);
    if (node->parent != invalid_entity) {
      const auto& parent_transform_component = repository_.transform_components.find(node->parent);
      local_transform->set_parent_position(parent_transform_component->position());
      local_transform->set_parent_rotation(parent_transform_component->rotation());
      local_transform->set_parent_scale(parent_transform_component->scale().x);
    } else {
      local_transform->set_parent_position(local_transform->position());
      local_transform->set_parent_rotation( local_transform->rotation());
      local_transform->set_parent_scale(local_transform->scale().x);
    }

    const auto& mesh_component = repository_.mesh_components.find(entity);

    AABB aabb;
    auto& [min, max, is_dirty] = aabb;

    if (mesh_component != nullptr) {
      const auto& mesh = repository_.meshes.get(mesh_component->mesh);
      AABB local_aabb;
      local_aabb.min = mesh.local_bounds.center - glm::vec3(mesh.local_bounds.radius);
      local_aabb.max = mesh.local_bounds.center + glm::vec3(mesh.local_bounds.radius);

      min = glm::min(min, local_aabb.min);
      max = glm::max(max, local_aabb.max);
    } else {
      // nodes without mesh components should still influence the bounds
      min = glm::vec3(-0.0001f);
      max = glm::vec3(0.0001f);
    }

    const TransformComponent model_transform = parent_transform * *local_transform;
    for (const auto& child : node->children) {
      update_aabb(child, model_transform);
      const auto& child_aabb = repository_.scene_graph.find(child)->bounds;
      min.x = std::min(min.x, child_aabb.min.x);
      min.y = std::min(min.y, child_aabb.min.y);
      min.z = std::min(min.z, child_aabb.min.z);

      max.x = std::max(max.x, child_aabb.max.x);
      max.y = std::max(max.y, child_aabb.max.y);
      max.z = std::max(max.z, child_aabb.max.z);
    }

    // Decompose model_matrix into translation (T) and 3x3 rotation matrix (M)
    glm::vec3 T = model_transform.position();             // Translation vector
    glm::vec3 S = glm::vec3(model_transform.scale());                // Scale vector
    glm::mat3 M = glm::mat3(model_transform.rotation());  // Rotation matrix
    M = transpose(M);  // Otherwise the AABB will be rotated in the wrong direction

    AABB transformedAABB = {T, T};  // Start with a zero-volume AABB at T

    // Applying Arvo's method to adjust AABB based on rotation and translation
    // NOTE: does not work for non-uniform scaling
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        float a = M[i][j] * S[j] * min[j];
        float b = M[i][j] * S[j] * max[j];
        transformedAABB.min[i] += a < b ? a : b;
        transformedAABB.max[i] += a < b ? b : a;
      }
    }
    min = transformedAABB.min;
    max = transformedAABB.max;


    node->bounds = aabb;
    node->bounds.is_dirty = false;
  }

  void TransformSystem::update() {
    bool is_dirty = false;

    for (auto& [entity, transform] : repository_.transform_components.snapshot()) {
      if (transform.is_dirty) {
        is_dirty = true;
        mark_bounds_as_dirty(entity);
        transform.is_dirty = false;
      }
    }

    if (is_dirty) {
      const auto root_transform = TransformComponent();

      update_aabb(root_entity, root_transform);
    }
  }

}  // namespace gestalt::application