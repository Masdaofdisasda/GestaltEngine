#pragma once

#include <vk_types.h>

#include <filesystem>
#include <unordered_map>

// forward declaration
class VulkanEngine;

struct GLTFMaterial {
  MaterialInstance data;
};

struct GeoSurface {
  uint32_t startIndex;
  uint32_t count;
  std::shared_ptr<GLTFMaterial> material;
};

struct MeshAsset {
  std::string name;

  std::vector<GeoSurface> surfaces;
  GPUMeshBuffers meshBuffers;
};

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(
    VulkanEngine* engine, std::filesystem::path filePath);