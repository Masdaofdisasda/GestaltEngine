#include "resource_manager.h"
#include <stb_image.h>

#include "vk_images.h"
#include "vk_initializers.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

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

gpu_mesh_buffers resource_manager::upload_mesh(const std::span<uint32_t> indices, const std::span<Vertex> vertices) {
  //> mesh_create_1
  const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
  const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

  gpu_mesh_buffers newSurface;

  // create vertex buffer
  newSurface.vertexBuffer = create_buffer(
      vertexBufferSize,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
          | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY);

  // find the adress of the vertex buffer
  VkBufferDeviceAddressInfo deviceAdressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                             .buffer = newSurface.vertexBuffer.buffer};
  newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(gpu_.device, &deviceAdressInfo);

  // create index buffer
  newSurface.indexBuffer = create_buffer(
      indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY);

  //< mesh_create_1
  //
  //> mesh_create_2
  AllocatedBuffer staging = create_buffer(vertexBufferSize + indexBufferSize,
                                                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                            VMA_MEMORY_USAGE_CPU_ONLY);

  void* data;
  VmaAllocation allocation = staging.allocation;
  VK_CHECK(vmaMapMemory(gpu_.allocator, allocation, &data));

  // copy vertex buffer
  memcpy(data, vertices.data(), vertexBufferSize);
  // copy index buffer
  memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

  gpu_.immediate_submit([&](VkCommandBuffer cmd) {
    VkBufferCopy vertex_copy;
    vertex_copy.dstOffset = 0;
    vertex_copy.srcOffset = 0;
    vertex_copy.size = vertexBufferSize;

    vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertex_copy);

    VkBufferCopy index_copy;
    index_copy.dstOffset = 0;
    index_copy.srcOffset = vertexBufferSize;
    index_copy.size = indexBufferSize;

    vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &index_copy);
  });

  vmaUnmapMemory(gpu_.allocator, allocation);
  destroy_buffer(staging);

  return newSurface;
  //< mesh_create_2
}

void resource_manager::destroy_buffer(const AllocatedBuffer& buffer) {
  vmaDestroyBuffer(gpu_.allocator, buffer.buffer, buffer.allocation);
}


AllocatedImage resource_manager::create_image(VkExtent3D size, VkFormat format,
                                           VkImageUsageFlags usage, bool mipmapped) {
  AllocatedImage newImage;
  newImage.imageFormat = format;
  newImage.imageExtent = size;

  VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
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
      size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
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

            const std::string path(filePath.uri.path().begin(),
                                   filePath.uri.path().end());  // Thanks C++.
            unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
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

void resource_manager::destroy_image(const AllocatedImage& img) {
  vkDestroyImageView(gpu_.device, img.imageView, nullptr);
  vmaDestroyImage(gpu_.allocator, img.image, img.allocation);
}