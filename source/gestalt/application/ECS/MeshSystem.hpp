#pragma once

#include "Repository.hpp"
#include "BaseSystem.hpp"

namespace gestalt::application {

    class MeshSystem final : public BaseSystem, public NonCopyable<MeshSystem> {
      size_t meshes_ = 0;
      void traverse_scene(Entity entity, const TransformComponent& parent_transform);
      void upload_mesh();

      void create_buffers();

    public:
      void prepare() override;
      void update(float delta_time, const UserInput& movement, float aspect) override;
      void cleanup() override;
    };

}  // namespace gestalt