
#include <stb_image.h>
#include <iostream>
#include <vk_loader.h>

#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_types.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

#include "camera.h"

VkFilter extract_filter(fastgltf::Filter filter) {
  switch (filter) {
    // nearest samplers
    case fastgltf::Filter::Nearest:
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::NearestMipMapLinear:
      return VK_FILTER_NEAREST;

    // linear samplers
    case fastgltf::Filter::Linear:
    case fastgltf::Filter::LinearMipMapNearest:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
      return VK_FILTER_LINEAR;
  }
}

VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter) {
  switch (filter) {
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::LinearMipMapNearest:
      return VK_SAMPLER_MIPMAP_MODE_NEAREST;

    case fastgltf::Filter::NearestMipMapLinear:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
      return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  }
}

std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(render_engine* engine,
                                                    std::string_view filePath) {
  //> load_1
  fmt::print("Loading GLTF: {}", filePath);

  std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
  scene->creator = engine;
  LoadedGLTF& file = *scene.get();

  fastgltf::Parser parser{};

  constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember
                               | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers
                               | fastgltf::Options::LoadExternalBuffers;
  // fastgltf::Options::LoadExternalImages;

  fastgltf::GltfDataBuffer data;
  data.loadFromFile(filePath);

  fastgltf::Asset gltf;

  std::filesystem::path path = filePath;

  auto type = fastgltf::determineGltfFileType(&data);
  if (type == fastgltf::GltfType::glTF) {
    auto load = parser.loadGLTF(&data, path.parent_path(), gltfOptions);
    if (load) {
      gltf = std::move(load.get());
    } else {
      std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
      return {};
    }
  } else if (type == fastgltf::GltfType::GLB) {
    auto load = parser.loadBinaryGLTF(&data, path.parent_path(), gltfOptions);
    if (load) {
      gltf = std::move(load.get());
    } else {
      std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
      return {};
    }
  } else {
    std::cerr << "Failed to determine glTF container" << std::endl;
    return {};
  }
  //< load_1
  //> load_2
  // we can stimate the descriptors we will need accurately
  std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes
      = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}};

  file.descriptorPool.init(engine->get_gpu().device, gltf.materials.size(), sizes);
  //< load_2
  //> load_samplers

  // load samplers
  for (fastgltf::Sampler& sampler : gltf.samplers) {
    VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
    sampl.maxLod = VK_LOD_CLAMP_NONE;
    sampl.minLod = 0;

    sampl.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
    sampl.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

    sampl.mipmapMode = extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

    VkSampler newSampler;
    vkCreateSampler(engine->get_gpu().device, &sampl, nullptr, &newSampler);

    file.samplers.push_back(newSampler);
  }
  //< load_samplers
  //> load_arrays
  // temporal arrays for all the objects to use while creating the GLTF data
  std::vector<std::shared_ptr<MeshAsset>> meshes;
  std::vector<std::shared_ptr<Node>> nodes;
  std::vector<AllocatedImage> images;
  std::vector<std::shared_ptr<GLTFMaterial>> materials;
  //< load_arrays

  // load all textures
  for (fastgltf::Image& image : gltf.images) {
    std::optional<AllocatedImage> img = load_image(engine, gltf, image);

    if (img.has_value()) {
      images.push_back(*img);
      file.images[image.name.c_str()] = *img;
    } else {
      // we failed to load, so lets give the slot a default white texture to not
      // completely break loading
      images.push_back(engine->get_default_material().error_checkerboard_image);
      std::cout << "gltf failed to load texture " << image.name << std::endl;
    }
  }

  //> load_buffer
  // create buffer to hold the material data
  file.materialDataBuffer = engine->get_resource_manager().create_buffer(
      sizeof(gltf_metallic_roughness::MaterialConstants) * gltf.materials.size(),
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  int data_index = 0;
  gltf_metallic_roughness::MaterialConstants* sceneMaterialConstants
      = (gltf_metallic_roughness::MaterialConstants*)file.materialDataBuffer.info.pMappedData;
  //< load_buffer
  //
  //> load_material
  for (fastgltf::Material& mat : gltf.materials) {
    std::shared_ptr<GLTFMaterial> newMat = std::make_shared<GLTFMaterial>();
    materials.push_back(newMat);
    file.materials[mat.name.c_str()] = newMat;

    gltf_metallic_roughness::MaterialConstants constants;
    constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
    constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
    constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
    constants.colorFactors.w = mat.pbrData.baseColorFactor[3];

    constants.metal_rough_factors.x = mat.pbrData.metallicFactor;
    constants.metal_rough_factors.y = mat.pbrData.roughnessFactor;
    // write material parameters to buffer
    sceneMaterialConstants[data_index] = constants;

    MaterialPass passType = MaterialPass::MainColor;
    if (mat.alphaMode == fastgltf::AlphaMode::Blend) {
      passType = MaterialPass::Transparent;
    }

    gltf_metallic_roughness::MaterialResources materialResources;
    // default the material textures
    materialResources.colorImage = engine->get_default_material().white_image;
    materialResources.colorSampler = engine->get_default_material().default_sampler_linear;
    materialResources.metalRoughImage = engine->get_default_material().white_image;
    materialResources.metalRoughSampler = engine->get_default_material().default_sampler_linear;

    // set the uniform buffer for the material data
    materialResources.dataBuffer = file.materialDataBuffer.buffer;
    materialResources.dataBufferOffset
        = data_index * sizeof(gltf_metallic_roughness::MaterialConstants);
    // grab textures from gltf file
    if (mat.pbrData.baseColorTexture.has_value()) {
      size_t img
          = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
      size_t sampler
          = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

      materialResources.colorImage = images[img];
      materialResources.colorSampler = file.samplers[sampler];
    }
    // build material
    newMat->data = engine->get_metallic_roughness_material().write_material(
        engine->get_gpu().device, passType, materialResources, file.descriptorPool);

    data_index++;
  }
  //< load_material

  // use the same vectors for all meshes so that the memory doesnt reallocate as
  // often
  std::vector<uint32_t> indices;
  std::vector<Vertex> vertices;

  for (fastgltf::Mesh& mesh : gltf.meshes) {
    std::shared_ptr<MeshAsset> newmesh = std::make_shared<MeshAsset>();
    meshes.push_back(newmesh);
    file.meshes[mesh.name.c_str()] = newmesh;
    newmesh->name = mesh.name;

    // clear the mesh arrays each mesh, we dont want to merge them by error
    indices.clear();
    vertices.clear();

    for (auto&& p : mesh.primitives) {
      GeoSurface newSurface;
      newSurface.startIndex = (uint32_t)indices.size();
      newSurface.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

      size_t initial_vtx = vertices.size();

      // load indexes
      {
        fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
        indices.reserve(indices.size() + indexaccessor.count);

        fastgltf::iterateAccessor<std::uint32_t>(
            gltf, indexaccessor, [&](std::uint32_t idx) { indices.push_back(idx + initial_vtx); });
      }

      // load vertex positions
      {
        fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
        vertices.resize(vertices.size() + posAccessor.count);

        fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                                                      [&](glm::vec3 v, size_t index) {
                                                        Vertex newvtx;
                                                        newvtx.position = v;
                                                        newvtx.normal = {1, 0, 0};
                                                        newvtx.color = glm::vec4{1.f};
                                                        newvtx.uv_x = 0;
                                                        newvtx.uv_y = 0;
                                                        vertices[initial_vtx + index] = newvtx;
                                                      });
      }

      // load vertex normals
      auto normals = p.findAttribute("NORMAL");
      if (normals != p.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec3>(
            gltf, gltf.accessors[(*normals).second],
            [&](glm::vec3 v, size_t index) { vertices[initial_vtx + index].normal = v; });
      }

      // load UVs
      auto uv = p.findAttribute("TEXCOORD_0");
      if (uv != p.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
                                                      [&](glm::vec2 v, size_t index) {
                                                        vertices[initial_vtx + index].uv_x = v.x;
                                                        vertices[initial_vtx + index].uv_y = v.y;
                                                      });
      }

      // load vertex colors
      auto colors = p.findAttribute("COLOR_0");
      if (colors != p.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec4>(
            gltf, gltf.accessors[(*colors).second],
            [&](glm::vec4 v, size_t index) { vertices[initial_vtx + index].color = v; });
      }

      if (p.materialIndex.has_value()) {
        newSurface.material = materials[p.materialIndex.value()];
      } else {
        newSurface.material = materials[0];
      }

      glm::vec3 minpos = vertices[initial_vtx].position;
      glm::vec3 maxpos = vertices[initial_vtx].position;
      for (int i = initial_vtx; i < vertices.size(); i++) {
        minpos = glm::min(minpos, vertices[i].position);
        maxpos = glm::max(maxpos, vertices[i].position);
      }

      newSurface.bounds.origin = (maxpos + minpos) / 2.f;
      newSurface.bounds.extents = (maxpos - minpos) / 2.f;
      newSurface.bounds.sphereRadius = length(newSurface.bounds.extents);
      newmesh->surfaces.push_back(newSurface);
    }

    newmesh->meshBuffers = engine->upload_mesh(indices, vertices);
  }
  //> load_nodes
  // load all nodes and their meshes
  for (fastgltf::Node& node : gltf.nodes) {
    std::shared_ptr<Node> newNode;

    // find if the node has a mesh, and if it does hook it to the mesh pointer and allocate it with
    // the meshnode class
    if (node.meshIndex.has_value()) {
      newNode = std::make_shared<mesh_node>();
      static_cast<mesh_node*>(newNode.get())->mesh = meshes[*node.meshIndex];
    } else {
      newNode = std::make_shared<Node>();
    }

    nodes.push_back(newNode);
    file.nodes[node.name.c_str()];

    std::visit(fastgltf::visitor{[&](fastgltf::Node::TransformMatrix matrix) {
                                   memcpy(&newNode->localTransform, matrix.data(), sizeof(matrix));
                                 },
                                 [&](fastgltf::Node::TRS transform) {
                                   glm::vec3 tl(transform.translation[0], transform.translation[1],
                                                transform.translation[2]);
                                   glm::quat rot(transform.rotation[3], transform.rotation[0],
                                                 transform.rotation[1], transform.rotation[2]);
                                   glm::vec3 sc(transform.scale[0], transform.scale[1],
                                                transform.scale[2]);

                                   glm::mat4 tm = translate(glm::mat4(1.f), tl);
                                   glm::mat4 rm = toMat4(rot);
                                   glm::mat4 sm = scale(glm::mat4(1.f), sc);

                                   newNode->localTransform = tm * rm * sm;
                                 }},
               node.transform);
  }
  //< load_nodes
  //> load_graph
  // run loop again to setup transform hierarchy
  for (int i = 0; i < gltf.nodes.size(); i++) {
    fastgltf::Node& node = gltf.nodes[i];
    std::shared_ptr<Node>& sceneNode = nodes[i];

    for (auto& c : node.children) {
      sceneNode->children.push_back(nodes[c]);
      nodes[c]->parent = sceneNode;
    }
  }

  // find the top nodes, with no parents
  for (auto& node : nodes) {
    if (node->parent.lock() == nullptr) {
      file.topNodes.push_back(node);
      node->refreshTransform(glm::mat4{1.f});
    }
  }
  return scene;
  //< load_graph
}

void LoadedGLTF::Draw(const glm::mat4& topMatrix, draw_context& ctx) {
  // create renderables from the scenenodes
  for (auto& n : topNodes) {
    n->Draw(topMatrix, ctx);
  }
}

void LoadedGLTF::clearAll() {
  VkDevice dv = creator->get_gpu().device;

  for (auto& [k, v] : meshes) {
    creator->get_resource_manager().destroy_buffer(v->meshBuffers.indexBuffer);
    creator->get_resource_manager().destroy_buffer(v->meshBuffers.vertexBuffer);
  }

  for (auto& [k, v] : images) {
    if (v.image == creator->get_default_material().error_checkerboard_image.image) {
      // dont destroy the default images
      continue;
    }
    creator->get_resource_manager().destroy_image(v);
  }

  for (auto& sampler : samplers) {
    vkDestroySampler(dv, sampler, nullptr);
  }

  auto materialBuffer = materialDataBuffer;
  auto samplersToDestroy = samplers;

  descriptorPool.destroy_pools(dv);

  creator->get_resource_manager().destroy_buffer(materialBuffer);
}

std::optional<AllocatedImage> load_image(render_engine* engine, fastgltf::Asset& asset,
                                         fastgltf::Image& image) {
  AllocatedImage newImage{};

  int width, height, nrChannels;

  std::visit(fastgltf::visitor{
                 [](auto& arg) {},
                 [&](fastgltf::sources::URI& filePath) {
                   assert(filePath.fileByteOffset == 0);  // We don't support offsets with stbi.
                   assert(filePath.uri.isLocalPath());    // We're only capable of loading
                                                          // local files.

                   const std::string path(filePath.uri.path().begin(),
                                          filePath.uri.path().end());  // Thanks C++.
                   unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
                   if (data) {
                     VkExtent3D imagesize;
                     imagesize.width = width;
                     imagesize.height = height;
                     imagesize.depth = 1;

                     newImage = engine->get_resource_manager().create_image(
                         data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                                     VK_IMAGE_USAGE_SAMPLED_BIT, true);

                     stbi_image_free(data);
                   }
                 },
                 [&](fastgltf::sources::Vector& vector) {
                   unsigned char* data = stbi_load_from_memory(
                       vector.bytes.data(), static_cast<int>(vector.bytes.size()), &width, &height,
                       &nrChannels, 4);
                   if (data) {
                     VkExtent3D imagesize;
                     imagesize.width = width;
                     imagesize.height = height;
                     imagesize.depth = 1;

                     newImage = engine->get_resource_manager().create_image(
                         data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                                     VK_IMAGE_USAGE_SAMPLED_BIT, true);

                     stbi_image_free(data);
                   }
                 },
                 [&](fastgltf::sources::BufferView& view) {
                   auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                   auto& buffer = asset.buffers[bufferView.bufferIndex];

                   std::visit(fastgltf::visitor{// We only care about VectorWithMime here, because
                                                // we specify LoadExternalBuffers, meaning all
                                                // buffers are already loaded into a vector.
                                                [](auto& arg) {},
                                                [&](fastgltf::sources::Vector& vector) {
                                                  unsigned char* data = stbi_load_from_memory(
                                                      vector.bytes.data() + bufferView.byteOffset,
                                                      static_cast<int>(bufferView.byteLength),
                                                      &width, &height, &nrChannels, 4);
                                                  if (data) {
                                                    VkExtent3D imagesize;
                                                    imagesize.width = width;
                                                    imagesize.height = height;
                                                    imagesize.depth = 1;

                                                    newImage
                                                        = engine->get_resource_manager()
                                                              .create_image(
                                                        data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                                        VK_IMAGE_USAGE_SAMPLED_BIT, true);

                                                    stbi_image_free(data);
                                                  }
                                                }},
                              buffer.data);
                 },
             },
             image.data);

  // if any of the attempts to load the data failed, we havent written the image
  // so handle is null
  if (newImage.image == VK_NULL_HANDLE) {
    return {};
  }
  return newImage;
}

void vk_scene_manager::load_scene_from_gltf(const std::string& filename) {

  //TODO load from file and parse

  std::vector<Vertex> vertices{4};
  vertices[0].position = {-1.f, -1.f, 0.f};
  vertices[0].color = glm::vec4(1.f);
  vertices[0].normal = {0.f, 0.f, 1.f};
  vertices[0].uv_x = 0.f;
  vertices[0].uv_y = 0.f;
  vertices[1].position = {1.f, -1.f, 0.f};
  vertices[1].color = glm::vec4(1.f);
  vertices[1].normal = {0.f, 0.f, 1.f};
  vertices[1].uv_x = 1.f;
  vertices[1].uv_y = 0.f;
  vertices[2].position = {1.f, 1.f, 0.f};
  vertices[2].color = glm::vec4(1.f);
  vertices[2].normal = {0.f, 0.f, 1.f};
  vertices[2].uv_x = 1.f;
  vertices[2].uv_y = 1.f;
  vertices[3].position = {-1.f, 1.f, 0.f};
  vertices[3].color = glm::vec4(1.f);
  vertices[3].normal = {0.f, 0.f, 1.f};
  vertices[3].uv_x = 0.f;
  vertices[3].uv_y = 1.f;

  std::vector<uint32_t> indices = {
      0, 1, 2, 2, 3, 0,
  };

  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < 10; ++j) {
      entity entity = create_entity();  // todo maybe use builder pattern?
      add_mesh_component(entity, vertices, indices);
      add_transform_component(entity, glm::vec3(i-5.f, j-5.f, 10.f), glm::quat(0.f, 0.f, 0.f, 0.f), glm::vec3(0.4f));
      root_.children.push_back(entity);
      add_material_component(entity, default_data_);
      // load textures and so on
    }
  }

  // after all meshes are loaded:
  mesh_buffers_ = resource_manager_.upload_mesh(indices_, vertices_);

}

void vk_scene_manager::add_transform_component(entity entity, const glm::vec3& position,
                                               const glm::quat& rotation, const glm::vec3& scale) {
  const transform_component transform{position, rotation, scale};
  transforms_.push_back(transform);
  scene_object& object = entity_hierarchy_[entity];
  object.transform = transforms_.size() - 1;
}

void vk_scene_manager::add_mesh_component(const entity entity, std::vector<Vertex>& vertices,
    std::vector<uint32_t>& indices) {

  mesh_component mesh;
  mesh.vertex_offset = vertices_.size();
  mesh.first_index = indices_.size();
  mesh.vertex_count = vertices.size();
  mesh.index_count = indices.size();

  glm::vec3 minpos = vertices[0].position;
  glm::vec3 maxpos = vertices[0].position;
  for (auto& vertice : vertices) {
    minpos = min(minpos, vertice.position);
    maxpos = max(maxpos, vertice.position);
  }

  mesh.bounds.origin = (maxpos + minpos) / 2.f;
  mesh.bounds.extents = (maxpos - minpos) / 2.f;
  mesh.bounds.sphereRadius = length(mesh.bounds.extents);

  vertices_.insert(vertices_.end(), vertices.begin(), vertices.end());
  indices_.insert(indices_.end(), indices.begin(), indices.end());

  meshes_.push_back(mesh);
  scene_object& object = entity_hierarchy_[entity];
  object.mesh = meshes_.size() - 1;
}

const std::vector<entity>& vk_scene_manager::get_children(entity entity) const {
  return entity_hierarchy_.at(entity).children;
}

void vk_scene_manager::update_scene(draw_context& draw_context) {

  // get root
  for (const auto& nodes = root_.children; auto& entity : nodes) {

    const auto& object = entity_hierarchy_.at(entity);
    const auto& mesh = meshes_.at(object.mesh);
    const auto& transform = transforms_.at(object.transform);
    auto& material = materials_.at(object.material);

    render_object def;
    def.index_count = mesh.index_count;
    def.first_index = mesh.first_index;
    def.index_buffer = mesh_buffers_.indexBuffer.buffer;
    def.material = &material.data;
    def.bounds = mesh.bounds;
    def.transform = transform.getModelMatrix();
    def.vertex_buffer_address = mesh_buffers_.vertexBufferAddress;

    draw_context.opaque_surfaces.push_back(def);
  }
  // repeat for all child nodes
}


void vk_scene_manager::init_default_data() {

  // 3 default textures, white, grey, black. 1 pixel each
  uint32_t white = 0xFFFFFFFF;
  default_material_.white_image = resource_manager_.create_image(
      (void*)&white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

  uint32_t grey = 0xAAAAAAFF;
  default_material_.grey_image = resource_manager_.create_image(
      (void*)&grey, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

  uint32_t black = 0x000000FF;
  default_material_.black_image = resource_manager_.create_image(
      (void*)&black, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

  // checkerboard image
  uint32_t magenta = 0xFF00FFFF;
  constexpr size_t checkerboard_size = 256;
  std::array<uint32_t, checkerboard_size> pixels;  // for 16x16 checkerboard texture
  for (int x = 0; x < 16; x++) {
    for (int y = 0; y < 16; y++) {
      pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
    }
  }
  default_material_.error_checkerboard_image = resource_manager_.create_image(
      pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

  VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

  sampl.magFilter = VK_FILTER_NEAREST;
  sampl.minFilter = VK_FILTER_NEAREST;

  vkCreateSampler(gpu_.device, &sampl, nullptr, &default_material_.default_sampler_nearest);

  sampl.magFilter = VK_FILTER_LINEAR;
  sampl.minFilter = VK_FILTER_LINEAR;
  vkCreateSampler(gpu_.device, &sampl, nullptr, &default_material_.default_sampler_linear);

  // create default material
  std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes
      = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}};

  descriptorPool.init(gpu_.device, 1, sizes);
  AllocatedBuffer materialDataBuffer = resource_manager_.create_buffer(
      sizeof(gltf_metallic_roughness::MaterialConstants),
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  int data_index = 0;
  gltf_metallic_roughness::MaterialConstants* sceneMaterialConstants
      = (gltf_metallic_roughness::MaterialConstants*)materialDataBuffer.info.pMappedData;

  gltf_metallic_roughness::MaterialConstants constants;
  constants.colorFactors.x = 1.f;
  constants.colorFactors.y = 1.f;
  constants.colorFactors.z = 1.f;
  constants.colorFactors.w = 1.f;

  constants.metal_rough_factors.x = 0.f;
  constants.metal_rough_factors.y = 0.f;
  // write material parameters to buffer
  sceneMaterialConstants[data_index] = constants;

  MaterialPass passType = MaterialPass::MainColor;

  gltf_metallic_roughness::MaterialResources materialResources;
  // default the material textures
  materialResources.colorImage = default_material_.error_checkerboard_image;
  materialResources.colorSampler = default_material_.default_sampler_linear;
  materialResources.metalRoughImage = default_material_.white_image;
  materialResources.metalRoughSampler = default_material_.default_sampler_linear;

  // set the uniform buffer for the material data
  materialResources.dataBuffer = materialDataBuffer.buffer;
  materialResources.dataBufferOffset
      = data_index * sizeof(gltf_metallic_roughness::MaterialConstants);

  // build material
  default_data_ = gltf_material_.write_material(
      gpu_.device, passType, materialResources, descriptorPool);

  deletion_service_.push_function(
      [this]() { resource_manager_.destroy_image(default_material_.white_image); });
  deletion_service_.push_function(
      [this]() { resource_manager_.destroy_image(default_material_.grey_image); });
  deletion_service_.push_function(
      [this]() { resource_manager_.destroy_image(default_material_.error_checkerboard_image); });
  deletion_service_.push(default_material_.default_sampler_nearest);
  deletion_service_.push(default_material_.default_sampler_linear);
}

void vk_scene_manager::init_renderables(render_engine& render_engine) {
  std::string structurePath
      = {R"(..\..\assets\Models\MetalRoughSpheres\glTF-Binary\MetalRoughSpheres.glb)"};
  auto structureFile = loadGltf(&render_engine, structurePath);

  assert(structureFile.has_value());

  loaded_scenes_["structure"] = *structureFile;
}