#pragma once

#include <vk_types.h>

#include <filesystem>
#include <unordered_map>
#include <fastgltf/types.hpp>

#include "vk_descriptors.h"

// forward declaration
class vulkan_engine;

struct Bounds {
  glm::vec3 origin;
  float sphereRadius;
  glm::vec3 extents;
};

struct GLTFMaterial {
  MaterialInstance data;
};

struct GeoSurface {
  uint32_t startIndex;
  uint32_t count;
  Bounds bounds;
  std::shared_ptr<GLTFMaterial> material;
};

struct MeshAsset {
  std::string name;

  std::vector<GeoSurface> surfaces;
  GPUMeshBuffers meshBuffers;
};

struct LoadedGLTF : public IRenderable {
  // storage for all the data on a given glTF file
  std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
  std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
  std::unordered_map<std::string, AllocatedImage> images;
  std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

  // nodes that dont have a parent, for iterating through the file in tree order
  std::vector<std::shared_ptr<Node>> topNodes;

  std::vector<VkSampler> samplers;

  DescriptorAllocatorGrowable descriptorPool;

  AllocatedBuffer materialDataBuffer;

  vulkan_engine* creator;

  ~LoadedGLTF() { clearAll(); };

  virtual void Draw(const glm::mat4& topMatrix, draw_context& ctx);

private:
  void clearAll();
};

std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(vulkan_engine* engine,
                                                    std::string_view filePath);

std::optional<AllocatedImage> load_image(vulkan_engine* engine, fastgltf::Asset& asset,
                                         fastgltf::Image& image);