#pragma once

#include "Repository.hpp"
#include "BaseSystem.hpp"

namespace gestalt::application {

    class MeshSystem final : public BaseSystem {
      size_t meshes_ = 0;
      void traverse_scene(Entity entity, const TransformComponent& parent_transform);
      void upload_mesh();

      void create_buffers();

    public:
      MeshSystem() = default;
      ~MeshSystem() override = default;

      MeshSystem(const MeshSystem&) = delete;
      MeshSystem& operator=(const MeshSystem&) = delete;

      MeshSystem(MeshSystem&&) = delete;
      MeshSystem& operator=(MeshSystem&&) = delete;

      void prepare() override;
      void update(float delta_time, const UserInput& movement, float aspect) override;
      void cleanup() override;
    };

}  // namespace gestalt