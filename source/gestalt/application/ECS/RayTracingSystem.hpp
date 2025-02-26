#pragma once

#include "Repository.hpp"
#include "BaseSystem.hpp"

namespace gestalt::application {

    class RayTracingSystem final : public BaseSystem {
      size_t meshes_ = 0;

      void build_blas();
      void build_tlas();
      void collect_tlas_instance_data(Entity entity, const TransformComponent& parent_transform,
                                      std::vector<VkAccelerationStructureInstanceKHR>& data);

    public:
      RayTracingSystem() = default;
      ~RayTracingSystem() override = default;

      RayTracingSystem(const RayTracingSystem&) = delete;
      RayTracingSystem& operator=(const RayTracingSystem&) = delete;

      RayTracingSystem(RayTracingSystem&&) = delete;
      RayTracingSystem& operator=(RayTracingSystem&&) = delete;

      void prepare() override;
      void update(float delta_time, const UserInput& movement, float aspect) override;
      void cleanup() override;
    };

}  // namespace gestalt