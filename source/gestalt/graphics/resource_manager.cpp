#include "resource_manager.h"

#include "vk_images.h"
#include "vk_initializers.h"

#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

#include "cubemap_util.h"

AllocatedBuffer resource_manager::create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                                VmaMemoryUsage memoryUsage) {
  // allocate buffer
  VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bufferInfo.pNext = nullptr;
  bufferInfo.size = allocSize;

  bufferInfo.usage = usage;

  VmaAllocationCreateInfo vmaallocInfo = {};
  vmaallocInfo.usage = memoryUsage;
  vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
  AllocatedBuffer newBuffer;

  // allocate the buffer
  VK_CHECK(vmaCreateBuffer(gpu_.allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer,
                           &newBuffer.allocation, &newBuffer.info));
  return newBuffer;
}

void resource_manager::init(const vk_gpu& gpu) {
  gpu_ = gpu;

  const uint32_t maxTextures = 1200;

  material_data_buffer_ = create_buffer(sizeof(gltf_material::MaterialConstants)
                          * maxTextures,  // TODO decide on material count dynamically
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  gltf_material::MaterialConstants defaultMaterialConstants = {};  // Populate with default values
  gltf_material::MaterialConstants* mappedData;
  vmaMapMemory(gpu_.allocator, material_data_buffer_.allocation, (void**)&mappedData);

  for (uint32_t i = 0; i < maxTextures; ++i) {
    memcpy(&mappedData[i], &defaultMaterialConstants, sizeof(gltf_material::MaterialConstants));
  }

  vmaUnmapMemory(gpu_.allocator, material_data_buffer_.allocation);


  writer.clear();
  writer.write_buffer(2, material_data_buffer_.buffer,
                      sizeof(gltf_material::MaterialConstants) * maxTextures,
                           0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

  materialLayout
      = descriptor_layout_builder()
            .add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT,
                         true)
            .build(gpu_.device);
  materialConstantsLayout = descriptor_layout_builder()
            .add_binding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, true)
            .build(gpu_.device);
  IblLayout = descriptor_layout_builder()
            .add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .add_binding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(gpu_.device);

  std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes
      = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxTextures},  // TODO
         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxTextures},
         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}};

  descriptorPool.init(gpu_.device, 1, sizes);
  std::vector<uint32_t> variableCounts = {maxTextures};
  materialSet = descriptorPool.allocate(gpu_.device, materialLayout, variableCounts);
  materialConstantsSet
      = descriptorPool.allocate(gpu_.device, materialConstantsLayout, variableCounts);
  IblSet = descriptorPool.allocate(gpu_.device, IblLayout);
  writer.update_set(gpu_.device, materialConstantsSet);
}

void resource_manager::cleanup() {
  destroy_buffer(material_data_buffer_);
  vkDestroyDescriptorSetLayout(gpu_.device, materialLayout, nullptr);
  descriptorPool.destroy_pools(gpu_.device);
}

// TODO : make this work with multiple meshes/scenes by resizing the buffers
void resource_manager::upload_mesh(const std::span<uint32_t> indices, const std::span<Vertex> vertices) {
  //> mesh_create_1
  const size_t vertex_buffer_size = vertices.size() * sizeof(Vertex);
  const size_t index_buffer_size = indices.size() * sizeof(uint32_t);

  // create vertex buffer
  scene_geometry_.vertexBuffer = create_buffer(
      vertex_buffer_size,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
          | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY);

  // find the adress of the vertex buffer
  VkBufferDeviceAddressInfo deviceAdressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                             .buffer = scene_geometry_.vertexBuffer.buffer};
  scene_geometry_.vertexBufferAddress = vkGetBufferDeviceAddress(gpu_.device, &deviceAdressInfo);

  // create index buffer
  scene_geometry_.indexBuffer = create_buffer(
      index_buffer_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY);

  //< mesh_create_1
  //
  //> mesh_create_2
  AllocatedBuffer staging = create_buffer(vertex_buffer_size + index_buffer_size,
                                                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                            VMA_MEMORY_USAGE_CPU_ONLY);

  void* data;
  VmaAllocation allocation = staging.allocation;
  VK_CHECK(vmaMapMemory(gpu_.allocator, allocation, &data));

  // copy vertex buffer
  memcpy(data, vertices.data(), vertex_buffer_size);
  // copy index buffer
  memcpy((char*)data + vertex_buffer_size, indices.data(), index_buffer_size);

  gpu_.immediate_submit([&](VkCommandBuffer cmd) {
    VkBufferCopy vertex_copy;
    vertex_copy.dstOffset = 0;
    vertex_copy.srcOffset = 0;
    vertex_copy.size = vertex_buffer_size;

    vkCmdCopyBuffer(cmd, staging.buffer, scene_geometry_.vertexBuffer.buffer, 1, &vertex_copy);

    VkBufferCopy index_copy;
    index_copy.dstOffset = 0;
    index_copy.srcOffset = vertex_buffer_size;
    index_copy.size = index_buffer_size;

    vkCmdCopyBuffer(cmd, staging.buffer, scene_geometry_.indexBuffer.buffer, 1, &index_copy);
  });

  vmaUnmapMemory(gpu_.allocator, allocation);
  destroy_buffer(staging);
  //< mesh_create_2
}

void resource_manager::destroy_buffer(const AllocatedBuffer& buffer) {
  vmaDestroyBuffer(gpu_.allocator, buffer.buffer, buffer.allocation);
}


AllocatedImage resource_manager::create_image(VkExtent3D size, VkFormat format,
                                           VkImageUsageFlags usage, bool mipmapped, bool cubemap) {
  AllocatedImage newImage;
  newImage.imageFormat = format;
  newImage.imageExtent = size;

  VkImageCreateInfo img_info;
  if (cubemap) {
   img_info =  vkinit::cubemap_create_info(format, usage, size);
  } else {
  img_info = vkinit::image_create_info(format, usage, size);
  }

  if (mipmapped) {
    img_info.mipLevels
        = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
  }

  // always allocate images on dedicated GPU memory
  VmaAllocationCreateInfo allocinfo = {};
  allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // allocate and create the image
  VK_CHECK(vmaCreateImage(gpu_.allocator, &img_info, &allocinfo, &newImage.image,
                          &newImage.allocation,
                          nullptr));

  // if the format is a depth format, we will need to have it use the correct
  // aspect flag
  VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
  if (format == VK_FORMAT_D32_SFLOAT) {
    aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
  }

  if (cubemap) {
    // Build an image-view for the image
    VkImageViewCreateInfo view_info
        = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
    view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;  // View type for cubemap
    view_info.subresourceRange.levelCount = img_info.mipLevels;
    view_info.subresourceRange.layerCount = 6;  // Layer count for cubemap

    VK_CHECK(vkCreateImageView(gpu_.device, &view_info, nullptr, &newImage.imageView));

    return newImage;
  }

  // build a image-view for the image
  VkImageViewCreateInfo view_info
      = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
  view_info.subresourceRange.levelCount = img_info.mipLevels;

  VK_CHECK(vkCreateImageView(gpu_.device, &view_info, nullptr, &newImage.imageView));

  return newImage;
}

AllocatedImage resource_manager::create_image(void* data, VkExtent3D size, VkFormat format,
                                           VkImageUsageFlags usage, bool mipmapped) {
  size_t data_size = static_cast<size_t>(size.depth * size.width * size.height) * 4;
  AllocatedBuffer uploadbuffer
      = create_buffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

  memcpy(uploadbuffer.info.pMappedData, data, data_size);

  AllocatedImage new_image = create_image(
      size, format,
      usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      mipmapped);

  gpu_.immediate_submit([&](VkCommandBuffer cmd) {
    vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;

    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = size;

    // copy the buffer into the image
    vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, new_image.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    if (mipmapped) {
      vkutil::generate_mipmaps(
          cmd, new_image.image,
          VkExtent2D{new_image.imageExtent.width, new_image.imageExtent.height});
    } else {
      vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
  });
  destroy_buffer(uploadbuffer);
  return new_image;
}

AllocatedImage resource_manager::create_cubemap(const void* imageData, VkExtent3D size,
                                                VkFormat format, VkImageUsageFlags usage,
                                                bool mipmapped) {
  size_t faceWidth = size.width;
  size_t faceHeight = size.height;
  size_t numChannels = 4;  // Assuming RGBA
  size_t bytesPerFloat = sizeof(float);

  size_t faceSizeBytes = faceWidth * faceHeight * numChannels * bytesPerFloat;
  size_t totalCubemapSizeBytes = faceSizeBytes * 6;

  // Create a buffer large enough to hold all faces
  AllocatedBuffer uploadbuffer = create_buffer(
      totalCubemapSizeBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                               VMA_MEMORY_USAGE_CPU_TO_GPU);

  // Copy each face data into the buffer
  memcpy(uploadbuffer.info.pMappedData, imageData, totalCubemapSizeBytes);

  // Create the image with VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT flag for cubemaps
  AllocatedImage new_image = create_image(
      size, format,
      usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      mipmapped, true);

  gpu_.immediate_submit([&](VkCommandBuffer cmd) {
    vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy each face from the buffer to the image
    for (int i = 0; i < 6; ++i) {
      VkBufferImageCopy copyRegion = {};
      copyRegion.bufferOffset = faceSizeBytes * i;
      copyRegion.bufferRowLength = 0;
      copyRegion.bufferImageHeight = 0;

      copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copyRegion.imageSubresource.mipLevel = 0;
      copyRegion.imageSubresource.baseArrayLayer = i;  // Specify the face index
      copyRegion.imageSubresource.layerCount = 1;
      copyRegion.imageExtent = size;

      vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, new_image.image,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
    }

    if (mipmapped) {
      //vkutil::generate_mipmaps( TODO
          //cmd, new_image.image,
          //VkExtent2D{new_image.imageExtent.width, new_image.imageExtent.height});
    } else {
      vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_IMAGE_LAYOUT_GENERAL);
    }
  });
  destroy_buffer(uploadbuffer);
  return new_image;
}

void resource_manager::write_material(const gltf_material& material,
                                      const uint32_t material_id) {
  gltf_material::MaterialResources resources = material.resources;
  gltf_material::MaterialConstants constants = material.constants;

  writer.clear();

  std::vector<VkDescriptorImageInfo> imageInfos
      = {{resources.colorSampler, resources.colorImage.imageView,
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
         {resources.metalRoughSampler, resources.metalRoughImage.imageView,
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
         {resources.normalSampler, resources.normalImage.imageView,
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
         {resources.emissiveSampler, resources.emissiveImage.imageView,
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
         {resources.occlusionSampler, resources.occlusionImage.imageView,
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};

  writer.write_image_array(1, imageInfos, imageInfos.size() * material_id);

  writer.update_set(gpu_.device, materialSet);

  gltf_material::MaterialConstants* mappedData;
  vmaMapMemory(gpu_.allocator, material_data_buffer_.allocation, (void**)&mappedData);

  memcpy(&mappedData[material_id], &constants, sizeof(gltf_material::MaterialConstants));

  vmaUnmapMemory(gpu_.allocator, material_data_buffer_.allocation);

  writer.clear();
  writer.write_buffer(2, material_data_buffer_.buffer, sizeof(gltf_material::MaterialConstants),
                      sizeof(gltf_material::MaterialConstants) * material_id,
                      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  //writer.update_set(gpu_.device, materialConstantsSet);
}

std::optional<AllocatedImage> resource_manager::load_image(fastgltf::Asset& asset, fastgltf::Image& image) {
  AllocatedImage newImage{};

  int width, height, nrChannels;

  std::visit(
      fastgltf::visitor{
          [](auto& arg) {},
          [&](fastgltf::sources::URI& filePath) {
            assert(filePath.fileByteOffset == 0);  // We don't support offsets with stbi.
            assert(filePath.uri.isLocalPath());    // We're only capable of loading
                                                   // local files.

            const std::string filename(filePath.uri.path().begin(),
                                   filePath.uri.path().end());  // todo get filenames relative for .gtlf
            //const std::string path = "../../assets/Models/Sponza/glTF/" + filename;
            unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrChannels, 4);
            if (data) {
              VkExtent3D imagesize;
              imagesize.width = width;
              imagesize.height = height;
              imagesize.depth = 1;

              newImage = create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                                       VK_IMAGE_USAGE_SAMPLED_BIT, true);

              stbi_image_free(data);
            }
          },
          [&](fastgltf::sources::Vector& vector) {
            unsigned char* data
                = stbi_load_from_memory(vector.bytes.data(), static_cast<int>(vector.bytes.size()),
                                        &width, &height, &nrChannels, 4);
            if (data) {
              VkExtent3D imagesize;
              imagesize.width = width;
              imagesize.height = height;
              imagesize.depth = 1;

              newImage = create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
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
                                               static_cast<int>(bufferView.byteLength), &width,
                                               &height, &nrChannels, 4);
                                           if (data) {
                                             VkExtent3D imagesize;
                                             imagesize.width = width;
                                             imagesize.height = height;
                                             imagesize.depth = 1;

                                             newImage = create_image(
                                                 data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                                 VK_IMAGE_USAGE_SAMPLED_BIT, true);

                                             stbi_image_free(data);
                                           }
                                         }},
                       buffer.data);
          },
      },
      image.data);

  // if any of the attempts to load the data failed, we haven't written the image
  // so handle is null
  if (newImage.image == VK_NULL_HANDLE) {
    return {};
  }
  return newImage;
}

static void float24to32(int w, int h, const float* img24, float* img32) {
  const int numPixels = w * h;
  for (int i = 0; i != numPixels; i++) {
    *img32++ = *img24++;
    *img32++ = *img24++;
    *img32++ = *img24++;
    *img32++ = 1.0f;
  }
}

void resource_manager::load_and_process_cubemap(const std::string& file_path) {
  int w, h, comp;
  const float* img = stbi_loadf(file_path.c_str(), &w, &h, &comp, 3);

  if (!img) {
      fmt::print("Failed to load image: {}\n", file_path);
    fflush(stdout);
    return;
  }

  {
    std::vector<float> img32(w * h * 4);

    float24to32(w, h, img, img32.data());

    environment_map = create_cubemap_from_HDR(img32, h, w);
  }

  //TODO bdrf convolution

  const int dstW = 256;
  const int dstH = 128;

  std::vector<glm::vec3> out(dstW * dstH);

  int numPoints = 1024;
  convolveDiffuse((glm::vec3*)img, w, h, dstW, dstH, out.data(), numPoints);

  stbi_image_free((void*)img);
  stbi_write_hdr("../../assets/filtered.hdr", dstW, dstH, 3, (float*)out.data());

  const float* filtered = stbi_loadf("../../assets/filtered.hdr", &w, &h, &comp, 3);
  std::vector<float> img32(w * h * 4);

  float24to32(w, h, filtered, img32.data());

  if (!filtered) {
    fmt::print("Failed to load image: {}\n", "../../assets/filtered.hdr");
    fflush(stdout);
  }

  stbi_image_free((void*)filtered);

  environment_irradiance_map = create_cubemap_from_HDR(img32, h, w);

  VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
  sampl.maxLod = VK_LOD_CLAMP_NONE;
  sampl.minLod = 0;
  sampl.magFilter = VK_FILTER_LINEAR;
  sampl.minFilter = VK_FILTER_LINEAR;
  sampl.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  vkCreateSampler(gpu_.device, &sampl, nullptr, &cube_map_sampler);

  writer.clear();
  writer.write_image(3, environment_map.imageView, cube_map_sampler,
                     VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.write_image(4, environment_irradiance_map.imageView,
                     cube_map_sampler, VK_IMAGE_LAYOUT_GENERAL,
                     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.update_set(gpu_.device, IblSet);
}

AllocatedImage resource_manager::create_cubemap_from_HDR(std::vector<float>& image_data, int h,
                                                         int w) {
  Bitmap in(w, h, 4, eBitmapFormat_Float, image_data.data());
  Bitmap out_bitmap = convertEquirectangularMapToVerticalCross(in);

  Bitmap cube = convertVerticalCrossToCubeMapFaces(out_bitmap);

  return create_cubemap(cube.data_.data(),
                                {static_cast<uint32_t>(cube.w_), static_cast<uint32_t>(cube.h_), 1},
                                VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, false);
}

void resource_manager::destroy_image(const AllocatedImage& img) {
  vkDestroyImageView(gpu_.device, img.imageView, nullptr);
  vmaDestroyImage(gpu_.allocator, img.image, img.allocation);
}
