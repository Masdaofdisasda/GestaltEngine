#pragma once

#include <filesystem>
#include "common.hpp"
#include "ECS/ComponentFactory.hpp"


namespace gestalt::foundation {
  struct GpuVertexData;
  struct GpuVertexPosition;
  struct Meshlet;
}

struct meshopt_Meshlet;

namespace gestalt::application {
  struct Vertex;


  class MeshProcessor {
  public:
    static void optimize_mesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);

    static void simplify_mesh(const std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                              const size_t max_indices);

    struct CompressedVertexData {
      std::vector<GpuVertexPosition> vertex_positions;
      std::vector<GpuVertexData> vertex_data;
    };

    static CompressedVertexData compress_vertex_data(const std::vector<Vertex>& vertices);

    struct MeshletData {
      std::vector<uint32> meshlet_vertices;
      std::vector<uint8> meshlet_indices;
      std::vector<Meshlet> meshlets;
    };

    static std::vector<meshopt_Meshlet> BuildMeshlets(
        const std::vector<uint32_t>& indices, const std::vector<GpuVertexPosition>& vertices,
        std::vector<uint32_t>& meshlet_vertices_local, std::vector<uint8_t>& meshlet_triangles);

    static std::vector<Meshlet> ComputeMeshletBounds(
        const std::vector<meshopt_Meshlet>& meshopt_meshlets,
        const std::vector<uint32_t>& meshlet_vertices_local,
        const std::vector<uint8_t>& meshlet_triangles,
        const std::vector<GpuVertexPosition>& vertices, size_t global_meshlet_vertex_offset,
        size_t global_meshlet_index_offset, size_t global_mesh_draw_count);

    static std::vector<uint8_t> ConvertAndStoreIndices(
        const std::vector<uint8_t>& meshlet_triangles);

    static std::vector<uint32_t> ConvertAndStoreVertices(
        const std::vector<uint32_t>& meshlet_vertices_local) {
      std::vector<uint32_t> meshlet_vertices;
      meshlet_vertices.reserve(meshlet_vertices_local.size());
      meshlet_vertices.insert(meshlet_vertices.end(), meshlet_vertices_local.begin(),
                              meshlet_vertices_local.end());
      return meshlet_vertices;
    }

    static MeshletData generate_meshlets(std::vector<GpuVertexPosition>& vertices,
                                         const std::vector<uint32_t>& indices,
                                         size_t global_mesh_draw_count,
                                         size_t global_meshlet_vertex_offset,
                                         size_t global_meshlet_index_offset);

    static MeshSurface create_surface(std::vector<GpuVertexPosition>& vertex_positions,
                                      std::vector<GpuVertexData>& vertex_data,
                                      std::vector<uint32_t>& indices,
                                      std::vector<Meshlet>&& meshlets,
                                      std::vector<uint32>&& meshlet_vertices,
                                      std::vector<uint8>&& meshlet_indices,
                                      Repository* repository);
  };
}  // namespace gestalt::application