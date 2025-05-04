#pragma once

#include "Repository.hpp"

namespace gestalt::foundation {
  struct FrameProvider;
  class IResourceAllocator;
}

namespace gestalt::application {

    class MeshSystem final {
    IGpu& gpu_;
    IResourceAllocator& resource_allocator_;
    Repository& repository_;
    FrameProvider& frame_;
      size_t meshes_ = 0;
      void traverse_scene(Entity entity, const TransformComponent& parent_transform);
      void upload_mesh();

      void create_buffers();

    public:
      MeshSystem(IGpu& gpu, IResourceAllocator& resource_allocator, Repository& repository,
                 FrameProvider& frame);
      ~MeshSystem();

      MeshSystem(const MeshSystem&) = delete;
      MeshSystem& operator=(const MeshSystem&) = delete;

      MeshSystem(MeshSystem&&) = delete;
      MeshSystem& operator=(MeshSystem&&) = delete;

      void update();
    };

}  // namespace gestalt