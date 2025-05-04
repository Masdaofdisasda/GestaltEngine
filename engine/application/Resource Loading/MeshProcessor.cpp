#include "MeshProcessor.hpp"

#include <fastgltf/glm_element_traits.hpp>

#include <meshoptimizer.h>

#include <functional>

#include "ECS/EntityComponentSystem.hpp"
#include "Mesh/MeshSurface.hpp"

namespace gestalt::application {

  struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 tangent;
    glm::vec2 uv;
  };


  void MeshProcessor::optimize_mesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
    // Step 1: Generate a remap table for vertex optimization
    std::vector<unsigned int> remap(vertices.size());
    size_t vertex_count
        = meshopt_generateVertexRemap(remap.data(), indices.data(), indices.size(),
                                      vertices.data(), vertices.size(), sizeof(Vertex));

    // Step 2: Remap indices
    std::vector<uint32_t> remappedIndices(indices.size());
    meshopt_remapIndexBuffer(remappedIndices.data(), indices.data(), indices.size(),
                             remap.data());

    // Step 3: Remap vertices
    std::vector<Vertex> remappedVertices(vertices.size());
    meshopt_remapVertexBuffer(remappedVertices.data(), vertices.data(), vertex_count,
                              sizeof(Vertex), remap.data());

    // Step 4: Vertex cache optimization
    meshopt_optimizeVertexCache(remappedIndices.data(), remappedIndices.data(),
                                remappedIndices.size(), vertex_count);

    // Step 5: Overdraw optimization (optional, depending on your needs)
    meshopt_optimizeOverdraw(remappedIndices.data(), remappedIndices.data(),
                             remappedIndices.size(),
                             reinterpret_cast<const float*>(remappedVertices.data()),
                             vertex_count, sizeof(Vertex), 1.05f);

    // Step 6: Vertex fetch optimization
    meshopt_optimizeVertexFetch(remappedVertices.data(), remappedIndices.data(),
                                remappedIndices.size(), remappedVertices.data(), vertex_count,
                                sizeof(Vertex));

    // Replace the original vectors with the optimized ones
    vertices.swap(remappedVertices);
    indices.swap(remappedIndices);
  }

  void MeshProcessor::simplify_mesh(const std::vector<Vertex>& vertices,
      std::vector<uint32_t>& indices, const size_t max_indices) {
    const size_t index_count = indices.size();

    if (index_count <= max_indices) {
      // No simplification needed
      return;
    }

    const size_t vertex_count = vertices.size();

    float complexity_threshold
        = static_cast<float>(max_indices) / static_cast<float>(index_count);
    complexity_threshold = std::min(complexity_threshold, 1.0f);

    const size_t target_index_count = index_count * complexity_threshold;
    constexpr float target_error = 1e-2f;
    constexpr unsigned int options = 0;

    std::vector<uint32_t> lod_indices(index_count);
    float lod_error = 0.0f;

    lod_indices.resize(meshopt_simplify(lod_indices.data(), indices.data(), index_count,
                                        reinterpret_cast<const float*>(vertices.data()),
                                        vertex_count, sizeof(Vertex), target_index_count,
                                        target_error, options, &lod_error));
    indices.swap(lod_indices);
  }

  MeshProcessor::CompressedVertexData MeshProcessor::compress_vertex_data(
      const std::vector<Vertex>& vertices) {
    std::vector<GpuVertexPosition> vertex_positions{vertices.size()};
    std::vector<GpuVertexData> vertex_data{vertices.size()};
    for (int i = 0; i < vertex_positions.size(); i++) {
      vertex_positions[i].position = vertices[i].position;

      vertex_data[i].normal[0] = static_cast<uint8_t>((vertices[i].normal.x + 1.0f) * 127.5f);
      vertex_data[i].normal[1] = static_cast<uint8_t>((vertices[i].normal.y + 1.0f) * 127.5f);
      vertex_data[i].normal[2] = static_cast<uint8_t>((vertices[i].normal.z + 1.0f) * 127.5f);
      vertex_data[i].normal[3] = 0; // padding

      vertex_data[i].tangent[0] = static_cast<uint8_t>((vertices[i].tangent.x + 1.0f) * 127.5f);
      vertex_data[i].tangent[1] = static_cast<uint8_t>((vertices[i].tangent.y + 1.0f) * 127.5f);
      vertex_data[i].tangent[2] = static_cast<uint8_t>((vertices[i].tangent.z + 1.0f) * 127.5f);
      vertex_data[i].tangent[3] = static_cast<uint8_t>((vertices[i].tangent.w + 1.0f)
                                                       * 127.5f); // Assuming .w is in [-1, 1]

      vertex_data[i].uv[0] = meshopt_quantizeHalf(vertices[i].uv.x);
      vertex_data[i].uv[1] = meshopt_quantizeHalf(vertices[i].uv.y);
    }

    return {vertex_positions, vertex_data};
  }

  std::vector<uint8_t> MeshProcessor::ConvertAndStoreIndices(
      const std::vector<uint8_t>& meshlet_triangles) {
    std::vector<uint8_t> meshlet_indices;
    meshlet_indices.reserve(meshlet_triangles.size());
    meshlet_indices.insert(meshlet_indices.end(), meshlet_triangles.begin(),
                           meshlet_triangles.end());
    return meshlet_indices;
  }

  MeshSurface MeshProcessor::create_surface(std::vector<GpuVertexPosition>& vertex_positions,
      std::vector<GpuVertexData>& vertex_data, std::vector<uint32_t>& indices,
      std::vector<Meshlet>&& meshlets, std::vector<uint32>&& meshlet_vertices,
      std::vector<uint8>&& meshlet_indices, Repository* repository) {
    assert(!vertex_positions.empty() && !indices.empty());
    assert(vertex_positions.size() == vertex_data.size());

    const size_t vertex_count = vertex_positions.size();
    const size_t index_count = indices.size();

    glm::vec3 center(0.0f);

    for (const auto& vertex : vertex_positions) {
      center += vertex.position;
    }
    center /= static_cast<float>(vertex_positions.size());

    float radius = 0.0f;
    for (const auto& vertex : vertex_positions) {
      float distance = glm::distance(center, vertex.position);
      radius = std::max(radius, distance);
    }

    glm::vec3 min(FLT_MAX);
    glm::vec3 max(-FLT_MAX);

    for (const auto& vertex : vertex_positions) {
      min = glm::min(min, vertex.position);
      max = glm::max(max, vertex.position);
    }

    auto meshlet_count = meshlets.size();
    auto meshlet_offset = repository->meshlets.size();
    repository->meshlets.add(meshlets);
    repository->meshlet_vertices.add(meshlet_vertices);
    repository->meshlet_triangles.add(meshlet_indices);
    const auto mesh_draw = repository->mesh_draws.add(MeshDraw{});

    const MeshSurface surface{
        .meshlet_offset = static_cast<uint32>(meshlet_offset),
        .meshlet_count = static_cast<uint32>(meshlet_count),
        .vertex_count = static_cast<uint32>(vertex_count),
        .index_count = static_cast<uint32>(index_count),
        .first_index = static_cast<uint32>(repository->indices.size()),
        .vertex_offset = static_cast<uint32>(repository->vertex_positions.size()),
        .local_bounds = BoundingSphere{center, radius},
        .local_aabb = AABB{min, max},
        .mesh_draw = mesh_draw,
    };

    repository->vertex_positions.add(vertex_positions);
    repository->vertex_data.add(vertex_data);
    const uint32 highest_index = *std::max_element(indices.begin(), indices.end());
    assert(highest_index <= repository->vertex_positions.size());
    repository->indices.add(indices);

    return surface;
  }

  MeshProcessor::MeshletData MeshProcessor::generate_meshlets(
      std::vector<GpuVertexPosition>& vertices, const std::vector<uint32_t>& indices,
      size_t global_mesh_draw_count, size_t global_meshlet_vertex_offset,
      size_t global_meshlet_index_offset) {
    std::vector<uint32_t> meshlet_vertices_local;
    std::vector<uint8_t> meshlet_triangles;

    auto meshopt_meshlets
        = BuildMeshlets(indices, vertices, meshlet_vertices_local, meshlet_triangles);

    auto result_meshlets = ComputeMeshletBounds(
        meshopt_meshlets, meshlet_vertices_local, meshlet_triangles, vertices,
        global_meshlet_vertex_offset, global_meshlet_index_offset, global_mesh_draw_count);

    std::vector<uint8_t> meshlet_indices = ConvertAndStoreIndices(meshlet_triangles);
    std::vector<uint32_t> meshlet_vertices = ConvertAndStoreVertices(meshlet_vertices_local);

    return {meshlet_vertices, meshlet_indices, result_meshlets};
  }

  std::vector<Meshlet> MeshProcessor::ComputeMeshletBounds(
      const std::vector<meshopt_Meshlet>& meshopt_meshlets,
      const std::vector<uint32_t>& meshlet_vertices_local,
      const std::vector<uint8_t>& meshlet_triangles, const std::vector<GpuVertexPosition>& vertices,
      size_t global_meshlet_vertex_offset, size_t global_meshlet_index_offset,
      size_t global_mesh_draw_count) {
    std::vector<Meshlet> result_meshlets(meshopt_meshlets.size());

    for (size_t i = 0; i < meshopt_meshlets.size(); ++i) {
      const auto& local_meshlet = meshopt_meshlets[i];
      auto& meshlet = result_meshlets[i];

      meshopt_Bounds meshlet_bounds = meshopt_computeMeshletBounds(
          meshlet_vertices_local.data() + local_meshlet.vertex_offset,
          meshlet_triangles.data() + local_meshlet.triangle_offset, local_meshlet.triangle_count,
          reinterpret_cast<const float*>(vertices.data()), vertices.size(),
          sizeof(GpuVertexPosition));

      meshlet.vertex_offset = local_meshlet.vertex_offset + global_meshlet_vertex_offset;
      meshlet.index_offset = local_meshlet.triangle_offset + global_meshlet_index_offset;
      meshlet.vertex_count = local_meshlet.vertex_count;
      meshlet.triangle_count = local_meshlet.triangle_count;

      meshlet.center = glm::vec3{meshlet_bounds.center[0], meshlet_bounds.center[1],
                                 meshlet_bounds.center[2]};
      meshlet.radius = meshlet_bounds.radius;

      meshlet.cone_axis[0] = meshlet_bounds.cone_axis_s8[0];
      meshlet.cone_axis[1] = meshlet_bounds.cone_axis_s8[1];
      meshlet.cone_axis[2] = meshlet_bounds.cone_axis_s8[2];
      meshlet.cone_cutoff = meshlet_bounds.cone_cutoff_s8;

      meshlet.mesh_draw_index = static_cast<uint32_t>(global_mesh_draw_count);
    }
    return result_meshlets;
  }

  std::vector<meshopt_Meshlet> MeshProcessor::BuildMeshlets(const std::vector<uint32_t>& indices,
                                                            const std::vector<GpuVertexPosition>& vertices, std::vector<uint32_t>& meshlet_vertices_local,
                                                            std::vector<uint8_t>& meshlet_triangles) {
    constexpr size_t max_vertices = 64;
    constexpr size_t max_triangles = 64;
    constexpr float cone_weight = 0.5f;

    size_t max_meshlets = meshopt_buildMeshletsBound(indices.size(), max_vertices, max_triangles);

    meshlet_vertices_local.resize(max_meshlets * max_vertices);
    meshlet_triangles.resize(max_meshlets * max_triangles * 3);

    std::vector<meshopt_Meshlet> meshopt_meshlets(max_meshlets);
    size_t meshlet_count = meshopt_buildMeshlets(
        meshopt_meshlets.data(), meshlet_vertices_local.data(), meshlet_triangles.data(),
        indices.data(), indices.size(), reinterpret_cast<const float*>(vertices.data()),
        vertices.size(), sizeof(GpuVertexPosition), max_vertices, max_triangles, cone_weight);

    meshopt_meshlets.resize(meshlet_count);
    meshlet_vertices_local.resize(meshopt_meshlets.back().vertex_offset
                                  + meshopt_meshlets.back().vertex_count);
    meshlet_triangles.resize(meshopt_meshlets.back().triangle_offset
                             + ((meshopt_meshlets.back().triangle_count * 3 + 3) & ~3));

    return meshopt_meshlets;
  }
}  // namespace gestalt::application