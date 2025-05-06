#include "RayTracingSystem.hpp"

#include <numeric>
#include <ranges>

#include "VulkanCheck.hpp"

#include <fmt/core.h>

#include "FrameProvider.hpp"
#include "Interface/IGpu.hpp"
#include "Interface/IResourceAllocator.hpp"
#include "Mesh/MeshSurface.hpp"

namespace gestalt::application {

  RayTracingSystem::RayTracingSystem(IGpu& gpu, IResourceAllocator& resource_allocator,
                                     Repository& repository, FrameProvider& frame)
      : gpu_(gpu),
        resource_allocator_(resource_allocator),
        repository_(repository),
        frame_(frame)
  {}

  void RayTracingSystem::build_tlas() {
    // TODO: This is a TLAS build, not an update, and probably should be refactored to some other
    // place. But! It works 💀
    /**
     * TLAS "Update"
     */
    const auto root_transform = TransformComponent();

    std::vector<VkAccelerationStructureInstanceKHR> tlasInstances;
    collect_tlas_instance_data(root_entity, root_transform, tlasInstances);

    if (repository_.tlas != nullptr) {
      vkDestroyAccelerationStructureKHR(gpu_.getDevice(),
                                        repository_.tlas->acceleration_structure, nullptr);
    }
    repository_.tlas = std::make_shared<AccelerationStructure>();

    VkDeviceSize instancesSize = tlasInstances.size() * sizeof(VkAccelerationStructureInstanceKHR);
    const auto tlasInstanceScratchBuffer = resource_allocator_.create_buffer(
        BufferTemplate("TLAS Instances Scratch Buffer", instancesSize,
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                       0, VMA_MEMORY_USAGE_CPU_TO_GPU));

    repository_.tlas_instance_buffer = resource_allocator_.create_buffer(
        BufferTemplate("TLAS Instance Buffer", instancesSize,
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT
                           | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
                           | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                       0, VMA_MEMORY_USAGE_GPU_ONLY));

    // vmaCopyMemoryToAllocation(gpu_.getAllocator(), tlasInstances.data(),
    // tlasScratchBuffer->get_allocation(), 0, instancesSize);
    void* tlasInstanceStagingMapped;
    const VmaAllocation allocation = tlasInstanceScratchBuffer->get_allocation();

    VK_CHECK(vmaMapMemory(gpu_.getAllocator(), allocation, &tlasInstanceStagingMapped));
    std::memcpy(tlasInstanceStagingMapped, tlasInstances.data(),
                tlasInstances.size() * sizeof(MeshDraw));

    VkAccelerationStructureGeometryInstancesDataKHR geometryInstancesData = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .pNext = nullptr,
        .arrayOfPointers = VK_FALSE,
        .data = VkDeviceOrHostAddressConstKHR(tlasInstanceScratchBuffer->get_address()),
    };

    VkAccelerationStructureGeometryDataKHR tlasGeometryData = {};
    tlasGeometryData.instances = geometryInstancesData;

    VkAccelerationStructureGeometryKHR tlasGeometry
        = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
           .pNext = nullptr,
           .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
           .geometry = tlasGeometryData,
           .flags = 0,
        };

    VkAccelerationStructureBuildGeometryInfoKHR tlasBuildGeometryInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .pNext = nullptr,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .srcAccelerationStructure = {},
        .dstAccelerationStructure = {},
        .geometryCount = 1,
        .pGeometries = &tlasGeometry,
        .ppGeometries = nullptr,
        .scratchData = {},
    };

    VkAccelerationStructureBuildSizesInfoKHR tlasBuildSizesInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        .pNext = nullptr,
        .accelerationStructureSize = 0,
        .updateScratchSize = 0,
        .buildScratchSize = 0,
    };
    const uint32 instanceCount = static_cast<uint32>(tlasInstances.size());
    vkGetAccelerationStructureBuildSizesKHR(
        gpu_.getDevice(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &tlasBuildGeometryInfo,
        &instanceCount, &tlasBuildSizesInfo);

    repository_.tlas_storage_buffer = resource_allocator_.create_buffer(
        BufferTemplate("TLAS Storage Buffer", tlasBuildSizesInfo.accelerationStructureSize,
                       VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
                           | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                       0, VMA_MEMORY_USAGE_GPU_ONLY));

    VkAccelerationStructureCreateInfoKHR tlasCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .createFlags = 0,
        .buffer = repository_.tlas_storage_buffer->get_buffer_handle(),
        .offset = 0,
        .size = tlasBuildSizesInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .deviceAddress = 0,
    };

    vkCreateAccelerationStructureKHR(gpu_.getDevice(), &tlasCreateInfo, nullptr,
                                     &repository_.tlas->acceleration_structure);
    gpu_.set_debug_name("TLAS", VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR,
                         reinterpret_cast<uint64_t>(repository_.tlas->acceleration_structure));

    const auto tlasBuildScratchBuffer = resource_allocator_.create_buffer(BufferTemplate(
        "TLAS Scratch Buffer", tlasBuildSizesInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 0,
        VMA_MEMORY_USAGE_AUTO));

    tlasBuildGeometryInfo.dstAccelerationStructure = repository_.tlas->acceleration_structure;
    tlasBuildGeometryInfo.scratchData
        = VkDeviceOrHostAddressKHR(tlasBuildScratchBuffer->get_address());

    VkAccelerationStructureBuildRangeInfoKHR tlasBuildRangeInfo = {};
    tlasBuildRangeInfo.primitiveCount = instanceCount;
    std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> tlasBuildRanges
        = {&tlasBuildRangeInfo};

    gpu_.immediateSubmit([&](const VkCommandBuffer& commandBuffer) {
      // Copy instance data from staging to gpu-side buffer
      VkBufferCopy copyRegion = {};
      copyRegion.size = instancesSize;
      copyRegion.srcOffset = 0;
      copyRegion.dstOffset = 0;
      vkCmdCopyBuffer(commandBuffer, tlasInstanceScratchBuffer->get_buffer_handle(),
                      repository_.tlas_instance_buffer->get_buffer_handle(), 1, &copyRegion);

      // Build TLAS
      vkCmdBuildAccelerationStructuresKHR(commandBuffer, static_cast<uint32>(tlasBuildRanges.size()),
                                          &tlasBuildGeometryInfo, tlasBuildRanges.data());
    });

    // Get TLAS address
    const VkAccelerationStructureDeviceAddressInfoKHR tlasAddressInfo = {
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
      .pNext = nullptr,
      .accelerationStructure = repository_.tlas->acceleration_structure
  };
    auto tlasAddress = vkGetAccelerationStructureDeviceAddressKHR(gpu_.getDevice(), &tlasAddressInfo);
    repository_.tlas->address = tlasAddress;
  }

  void RayTracingSystem::build_blas() {
    auto* mesh_buffers = repository_.mesh_buffers.get();
    auto* ray_tracing_buffers = repository_.ray_tracing_buffer.get();
    auto& meshes = repository_.meshes.data();

    const size_t blasCount = std::accumulate(
        std::begin(meshes), std::end(meshes), size_t{0},
        [](const size_t sum, const Mesh& mesh) { return sum + mesh.surfaces.size(); });

    for (size_t index = 0; index < blasCount; index++) {
      ray_tracing_buffers->bottom_level_acceleration_structures.push_back(
          std::make_shared<AccelerationStructure>());
    }

    /**
     * BLAS Geometry Data
     */
    std::vector<uint32_t> primitiveCounts;
    std::vector<VkAccelerationStructureGeometryTrianglesDataKHR> triangleDatas;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRangeInfos;
    std::vector<std::string> blasNames;
    uint32_t blasIndex = 0;
    for (auto&& [i, mesh] : std::views::enumerate(meshes)) {
      for (auto&& [j, surface] : std::views::enumerate(mesh.surfaces)) {
        blasNames.push_back(fmt::format("{} (Surface {}) Bottom-Level AS", mesh.name, j));
        surface.bottom_level_as = blasIndex;
        blasIndex++;

        const VkDeviceAddress vertexAddress = mesh_buffers->vertex_position_buffer->get_address();
        const VkDeviceAddress indexAddress = mesh_buffers->index_buffer->get_address();
        const uint32_t nPrimitives = surface.index_count / 3;

        VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo = {};
        buildRangeInfo.firstVertex = 0;
        buildRangeInfo.primitiveCount = nPrimitives;
        buildRangeInfo.primitiveOffset = surface.first_index * sizeof(uint32_t);
        buildRangeInfo.transformOffset = 0;
        buildRangeInfos.push_back(buildRangeInfo);

        VkAccelerationStructureGeometryTrianglesDataKHR triangleData
            = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};

        triangleData.indexData = VkDeviceOrHostAddressConstKHR(indexAddress);
        triangleData.indexType = VK_INDEX_TYPE_UINT32;
        triangleData.vertexData = VkDeviceOrHostAddressConstKHR(vertexAddress);
        triangleData.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        triangleData.vertexStride = sizeof(GpuVertexPosition);
        triangleData.maxVertex = surface.vertex_count;
        triangleData.pNext = nullptr;

        triangleDatas.push_back(triangleData);
        primitiveCounts.push_back(nPrimitives);
      }
    }

    std::vector<VkAccelerationStructureGeometryKHR> blasGeometries;
    for (const auto& triangleData : triangleDatas) {
      VkAccelerationStructureGeometryDataKHR geometryData = {};
      geometryData.triangles = triangleData;

      VkAccelerationStructureGeometryKHR geometry = {
          .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
      };

      geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
      geometry.geometry = geometryData;
      geometry.pNext = nullptr;

      blasGeometries.push_back(geometry);
    }

    std::vector<VkAccelerationStructureBuildGeometryInfoKHR> buildGeometryInfos;
    for (const auto& inputGeometry : blasGeometries) {
      VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
          .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
          .pNext = nullptr,
          .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
          .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
          .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
          .srcAccelerationStructure = VK_NULL_HANDLE,
          .dstAccelerationStructure = VK_NULL_HANDLE,
          .geometryCount = 1,
          .pGeometries = &inputGeometry,
          .ppGeometries = nullptr,
          .scratchData = {},

      };

      buildGeometryInfos.push_back(buildInfo);
    }

    /**
     * Calculate BLAS Memory Requirements
     */
    std::vector<VkAccelerationStructureBuildSizesInfoKHR> buildSizesInfos(blasCount);

    // sType ugh...
    for (auto& buildSizesInfo : buildSizesInfos) {
      buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    }

    for (size_t i = 0; i < blasCount; i++) {
      vkGetAccelerationStructureBuildSizesKHR(
          gpu_.getDevice(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
          &buildGeometryInfos[i], &primitiveCounts[i], &buildSizesInfos[i]);
    }

    /**
     * Create BLAS Data and Staging Buffers, and calculate per BLAS offsets
     */

    /** gpu_.getAccelerationStructureProperties().minAccelerationStructureScratchOffsetAlignment;
     * Minimum alignment on a desktop 3060 was 128, however the validation says that the offset must
     * be a multiple of 256, so we're just gonna use 256 for now.
     */
    constexpr VkDeviceSize alignment = 256;

    const VkDeviceSize blasDataBufferSize = std::accumulate(
        std::begin(buildSizesInfos), std::end(buildSizesInfos), VkDeviceSize{0},
        [&alignment](const VkDeviceSize sum,
                     const VkAccelerationStructureBuildSizesInfoKHR& build_sizes_info) {
          return sum + (build_sizes_info.accelerationStructureSize + alignment - 1)
                 & ~(alignment - 1);
        });

    ray_tracing_buffers->bottom_level_acceleration_structure_buffer
        = resource_allocator_.create_buffer(
        BufferTemplate("BLAS Buffer", blasDataBufferSize,
                       VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
                           | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                       0, VMA_MEMORY_USAGE_AUTO));

    std::vector<VkDeviceSize> blasDataBufferOffsets;
    VkDeviceSize currentOffset = 0;  // Per BLAS accumulated offset
    VkDeviceSize maxAlignedBlasSize
        = 0;  // We need one Scratch Buffer of the largest aligned BLAS size
    for (const auto& buildSizesInfo : buildSizesInfos) {
      const auto alignedSize
          = (buildSizesInfo.accelerationStructureSize + alignment - 1) & ~(alignment - 1);

      blasDataBufferOffsets.push_back(currentOffset);
      currentOffset += alignedSize;

      maxAlignedBlasSize = std::max(maxAlignedBlasSize, alignedSize);
    }

    const auto scratchBuffer = resource_allocator_.create_buffer(BufferTemplate(
        "BLAS Scratch Buffer", blasDataBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 0,
        VMA_MEMORY_USAGE_AUTO));

    /**
     * Create BLAS
     */
    for (size_t i = 0; i < blasCount; i++) {
      const VkAccelerationStructureCreateInfoKHR create_info = {
          .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
          .buffer
          = ray_tracing_buffers->bottom_level_acceleration_structure_buffer->get_buffer_handle(),
          .offset = blasDataBufferOffsets[i],
          .size = buildSizesInfos[i].accelerationStructureSize,
          .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
      };

      vkCreateAccelerationStructureKHR(
          gpu_.getDevice(), &create_info, nullptr,
          &ray_tracing_buffers->bottom_level_acceleration_structures[i]->acceleration_structure);

      gpu_.set_debug_name(
          blasNames[i], VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR,
          reinterpret_cast<uint64_t>(ray_tracing_buffers->bottom_level_acceleration_structures[i]
                                         ->acceleration_structure));

      // Get BLAS address
      const VkAccelerationStructureDeviceAddressInfoKHR blas_address = {
          .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
          .pNext = nullptr,
          .accelerationStructure
          = ray_tracing_buffers->bottom_level_acceleration_structures[i]->acceleration_structure,
      };
      auto blasAddress
          = vkGetAccelerationStructureDeviceAddressKHR(gpu_.getDevice(), &blas_address);
      ray_tracing_buffers->bottom_level_acceleration_structures[i]->address = blasAddress;
    }

    /**
     * Prepare BLAS build
     */
    std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> buildRangeInfoPtrs;
    for (size_t i = 0; i < blasCount; i++) {
      buildRangeInfoPtrs.push_back(&buildRangeInfos[i]);

      buildGeometryInfos[i].dstAccelerationStructure
          = ray_tracing_buffers->bottom_level_acceleration_structures[i]->acceleration_structure;
      buildGeometryInfos[i].scratchData
          = VkDeviceOrHostAddressKHR(scratchBuffer->get_address() + blasDataBufferOffsets[i]);
    }

    gpu_.immediateSubmit([&](const VkCommandBuffer& commandBuffer) {
      vkCmdBuildAccelerationStructuresKHR(commandBuffer, static_cast<uint32>(buildRangeInfoPtrs.size()),
                                          buildGeometryInfos.data(), buildRangeInfoPtrs.data());
    });
  }

  void RayTracingSystem::collect_tlas_instance_data(
      Entity entity, const TransformComponent& parent_transform,
      std::vector<VkAccelerationStructureInstanceKHR>& data) {
    const auto node = repository_.scene_graph.find(entity);
    if (!node->visible) {
      return;
    }

    const auto localTransform = repository_.transform_components.find(entity);
    TransformComponent worldTransform = parent_transform * *localTransform;

    const auto meshComponent = repository_.mesh_components.find(entity);
    if (meshComponent != nullptr) {
      const auto& mesh = repository_.meshes.get(meshComponent->mesh);
      for (const auto& surface : mesh.surfaces) {
        if (surface.bottom_level_as != no_component) {
          auto blas_address
              = repository_.ray_tracing_buffer
                                   ->bottom_level_acceleration_structures[surface.bottom_level_as]->address;
          VkAccelerationStructureInstanceKHR instance = {};
          instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
          instance.mask = 0xff;
          instance.instanceShaderBindingTableRecordOffset = 0;
          instance.accelerationStructureReference = blas_address;

          glm::mat4 T = glm::translate(glm::mat4(1.0f), worldTransform.position());
          glm::mat4 R = glm::mat4_cast(worldTransform.rotation());
          glm::mat4 S = glm::scale(glm::mat4(1.0f), worldTransform.scale());
          glm::mat4 m = T * R * S;

          instance.transform = {
              m[0].x, m[1].x, m[2].x, m[3].x, m[0].y, m[1].y,
              m[2].y, m[3].y, m[0].z, m[1].z, m[2].z, m[3].z,
          };

          data.push_back(instance);
        }
      }
    }

    for (const auto& childEntity : node->children) {
      collect_tlas_instance_data(childEntity, worldTransform, data);
    }
  }

  void RayTracingSystem::update() {
    if (!isVulkanRayTracingEnabled()) {
      return;
    }

    if (repository_.meshes.size() != meshes_) {
      build_blas();
      build_tlas();
      meshes_ = repository_.meshes.size();
    }
  }

  
  RayTracingSystem::~RayTracingSystem() {
    if (!isVulkanRayTracingEnabled()) {
      return;
    }
    resource_allocator_.destroy_buffer(repository_.tlas_instance_buffer);
    resource_allocator_.destroy_buffer(repository_.tlas_storage_buffer);
    resource_allocator_.destroy_buffer(
        repository_.ray_tracing_buffer->bottom_level_acceleration_structure_buffer);
  }
}  // namespace gestalt::application