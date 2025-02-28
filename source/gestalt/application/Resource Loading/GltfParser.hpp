#pragma once

#include <filesystem>
#include <unordered_map>

#include "AssetLoader.hpp"
#include "Repository.hpp"
#include "Vertex.hpp"
#include "common.hpp"
#include "ECS/ComponentFactory.hpp"

namespace fastgltf {
  struct Node;
  struct Mesh;
  struct Primitive;
  struct Material;
  struct Image;
  class Asset;
}

namespace gestalt::foundation {
  class Repository;
}

namespace gestalt::application {

    
  class GltfParser {
    static std::vector<uint32_t> extract_indices(const fastgltf::Asset& gltf,
                                                 fastgltf::Primitive& surface);

    static std::vector<Vertex> extract_vertices(const fastgltf::Asset& gltf,
                                                fastgltf::Primitive& surface);

    static MeshSurface extract_mesh_surface(const fastgltf::Asset& gltf,
                                            fastgltf::Primitive& primitive, size_t material_offset,
                                            Repository* repository);

  public:
    static std::vector<MeshSurface> extract_mesh(const fastgltf::Asset& gltf, fastgltf::Mesh& mesh,
                                                 size_t material_offset, Repository* repository);

    static void create_nodes(fastgltf::Asset& gltf, const size_t& mesh_offset,
                             ComponentFactory* component_factory);

    static void build_hierarchy(std::vector<fastgltf::Node> nodes, const size_t& node_offset,
                                Repository* repository);

    static void link_orphans_to_root(Entity root, NodeComponent* root_node,
                                     const std::vector<std::pair<Entity, NodeComponent>>& nodes,
                                     Repository* repository);
  };

}  // namespace gestalt::application