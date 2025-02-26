#pragma once

#include "Repository.hpp"
#include "BaseSystem.hpp"

namespace gestalt::application {

    class RayTracingSystem final {
    IGpu& gpu_;
    IResourceAllocator& resource_allocator_;
    Repository& repository_;
    FrameProvider& frame_;
      size_t meshes_ = 0;

      void build_blas();
      void build_tlas();
      void collect_tlas_instance_data(Entity entity, const TransformComponent& parent_transform,
                                      std::vector<VkAccelerationStructureInstanceKHR>& data);

    public:
      RayTracingSystem(IGpu& gpu, IResourceAllocator& resource_allocator, Repository& repository,
                       FrameProvider& frame);
      ~RayTracingSystem();

      RayTracingSystem(const RayTracingSystem&) = delete;
      RayTracingSystem& operator=(const RayTracingSystem&) = delete;

      RayTracingSystem(RayTracingSystem&&) = delete;
      RayTracingSystem& operator=(RayTracingSystem&&) = delete;

      void update();
    };

}  // namespace gestalt