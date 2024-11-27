#include "ResourceManager.hpp"

#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <filesystem>

#include "BasicResourceLoader.hpp"
#include "VulkanCheck.hpp"
#include "vk_initializers.hpp"
#include "Interface/IGpu.hpp"
#include "Utils/CubemapUtils.hpp"


namespace gestalt::graphics {

    void ResourceManager::init(IGpu* gpu,
                               Repository* repository) {
      gpu_ = gpu;
      repository_ = repository;
      resource_loader_ = std::make_unique<BasicResourceLoader>();
      resource_loader_->init(gpu);
      generate_samplers();
    }

    void ResourceManager::cleanup() {
      resource_loader_->cleanup();
    }

    void ResourceManager::flush_loader() { resource_loader_->flush(); }

  void ResourceManager::SetDebugName(const std::string& name, const VkObjectType type, const VkDevice device,
                                     const uint64_t handle) const {
      if (name.empty()) {
        return;
      }
      VkDebugUtilsObjectNameInfoEXT nameInfo = {};
      nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
      nameInfo.objectType = type;
      nameInfo.objectHandle = handle;
      nameInfo.pObjectName = name.c_str();
      vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
    }

    std::shared_ptr<AllocatedBufferOld> ResourceManager::create_buffer(size_t allocSize,
                                                                    VkBufferUsageFlags usage,
                                                                    VmaMemoryUsage memoryUsage,
                                                                    std::string name)
    {
      usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

      VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
      bufferInfo.size = allocSize;
      bufferInfo.usage = usage;

      VmaAllocationCreateInfo vmaallocInfo = {};
      vmaallocInfo.usage = memoryUsage;
      vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

      AllocatedBufferOld new_buffer;
      new_buffer.usage = usage;
      new_buffer.memory_usage = memoryUsage;

      VK_CHECK(vmaCreateBuffer(gpu_->getAllocator(), &bufferInfo, &vmaallocInfo, &new_buffer.buffer,
                               &new_buffer.allocation, &new_buffer.info));

      SetDebugName( name, VK_OBJECT_TYPE_BUFFER, gpu_->getDevice(), reinterpret_cast<uint64_t>(new_buffer.buffer));

      const VkBufferDeviceAddressInfo device_address_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = new_buffer.buffer};
      new_buffer.address
          = vkGetBufferDeviceAddress(gpu_->getDevice(), &device_address_info);

      return std::make_shared<AllocatedBufferOld>(new_buffer);
    }

    // https://github.com/KhronosGroup/Vulkan-Samples/tree/main/samples/extensions/descriptor_buffer_basic
  inline size_t aligned_size(size_t size, size_t alignment) {
      return (size + alignment - 1) & ~(alignment - 1);
    }

  std::shared_ptr<DescriptorBuffer> ResourceManager::create_descriptor_buffer(
        VkDescriptorSetLayout descriptor_layout, uint32 numBindings, VkBufferUsageFlags usage, std::string name) {
      DescriptorBuffer descriptor_buffer{gpu_->getDescriptorBufferProperties(), gpu_->getAllocator(),
                                       gpu_->getDevice()};
      descriptor_buffer.usage |= usage;

      const VkDeviceSize descriptor_buffer_offset_alignment
          = gpu_->getDescriptorBufferProperties().descriptorBufferOffsetAlignment;
      vkGetDescriptorSetLayoutSizeEXT(gpu_->getDevice(), descriptor_layout, &descriptor_buffer.size);
      descriptor_buffer.size = aligned_size(descriptor_buffer.size, descriptor_buffer_offset_alignment);

      descriptor_buffer.bindings.reserve(numBindings);
      VkDeviceSize binding_offset = 0;

      for (uint32 i = 0; i < numBindings; ++i) {
        vkGetDescriptorSetLayoutBindingOffsetEXT(gpu_->getDevice(), descriptor_layout, i,
                                                 &binding_offset);
        descriptor_buffer.bindings.emplace_back(DescriptorBinding{
            .binding = i,
            .offset = binding_offset,
        });
      }

      VkBufferCreateInfo bufferInfo = {};
      bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      bufferInfo.pNext = nullptr;
      bufferInfo.size = descriptor_buffer.size;
      bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      bufferInfo.usage = descriptor_buffer.usage;

      VmaAllocationCreateInfo vmaallocInfo = {};
      vmaallocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
      vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
      VK_CHECK(vmaCreateBuffer(gpu_->getAllocator(), &bufferInfo, &vmaallocInfo,
                               &descriptor_buffer.buffer, &descriptor_buffer.allocation,
                               &descriptor_buffer.info));

      SetDebugName(name, VK_OBJECT_TYPE_BUFFER, gpu_->getDevice(),
                   reinterpret_cast<uint64_t>(descriptor_buffer.buffer));


      VkBufferDeviceAddressInfo deviceAddressInfo
          = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
      deviceAddressInfo.buffer = descriptor_buffer.buffer;
      descriptor_buffer.address = vkGetBufferDeviceAddress(gpu_->getDevice(), &deviceAddressInfo);

      return std::make_shared<DescriptorBuffer>(descriptor_buffer);
    }

    void ResourceManager::destroy_buffer(const std::shared_ptr<AllocatedBufferOld> buffer) {
      vmaDestroyBuffer(gpu_->getAllocator(), buffer->buffer, buffer->allocation);
    }
    void ResourceManager::destroy_descriptor_buffer(const std::shared_ptr<DescriptorBuffer> buffer) const {
      vmaDestroyBuffer(gpu_->getAllocator(), buffer->buffer, buffer->allocation);
    }

    VkSampler ResourceManager::create_sampler(const VkSamplerCreateInfo& sampler_create_info) const {
      VkSampler new_sampler;
      vkCreateSampler(gpu_->getDevice(), &sampler_create_info, nullptr, &new_sampler);
      return new_sampler;
    }

  void ResourceManager::generate_samplers() const {
      std::vector<SamplerConfig> permutations;
      std::vector filters = {VK_FILTER_NEAREST, VK_FILTER_LINEAR};
      std::vector mipmapModes
          = {VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_MIPMAP_MODE_LINEAR};
      std::vector addressModes
          = {VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
             VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER};
      std::vector anisotropies = {16.0f};
      std::vector anisotropyEnabled = {VK_FALSE, VK_TRUE};
      std::vector borderColors = {VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK};

      for (auto magFilter : filters) {
        for (auto minFilter : filters) {
          for (auto mipmapMode : mipmapModes) {
            for (auto addressModeU : addressModes) {
              for (auto addressModeV : addressModes) {
                for (auto addressModeW : addressModes) {
                  for (auto maxAnisotropy : anisotropies) {
                    for (auto anisotropyEnable : anisotropyEnabled) {
                      SamplerConfig config
                          = {magFilter,    minFilter,    mipmapMode,    addressModeU,
                             addressModeV, addressModeW, maxAnisotropy, anisotropyEnable};
                      permutations.push_back(config);
                    }
                  }
                }
              }
            }
          }
        }
      }

        VkSamplerCreateInfo sampler_info = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
      for (const auto config : permutations) {
        sampler_info.magFilter = config.magFilter;
        sampler_info.minFilter = config.minFilter;
        sampler_info.mipmapMode = config.mipmapMode;
        sampler_info.addressModeU = config.addressModeU;
        sampler_info.addressModeV = config.addressModeV;
        sampler_info.addressModeW = config.addressModeW;
        sampler_info.mipLodBias = 0.0f;
        sampler_info.anisotropyEnable = config.anisotropyEnable;
        sampler_info.maxAnisotropy = config.maxAnisotropy;
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.compareOp = VK_COMPARE_OP_NEVER;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = VK_LOD_CLAMP_NONE;
        sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        sampler_info.unnormalizedCoordinates = VK_FALSE;

        VkSampler new_sampler;
        vkCreateSampler(gpu_->getDevice(), &sampler_info, nullptr, &new_sampler);
        repository_->sampler_cache[config] = new_sampler;
      }

    }

    TextureHandleOld ResourceManager::create_image(VkExtent3D size, VkFormat format,
                                                 VkImageUsageFlags usage, bool mipmapped,
                                                 bool cubemap) {

      TextureHandleOld newImage{cubemap ? TextureType::kCubeMap : TextureType::kColor};
      newImage.setFormat(format);
      newImage.imageExtent = size;

      VkImageCreateInfo img_info;
      if (cubemap) {
        img_info = vkinit::cubemap_create_info(format, usage, size);
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
      allocinfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

      // allocate and create the image
      VK_CHECK(vmaCreateImage(gpu_->getAllocator(), &img_info, &allocinfo, &newImage.image,
                              &newImage.allocation, nullptr));

      // if the format is a depth format, we will need to have it use the correct
      // aspect flag
      VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
      switch (format) {
        // Depth-only formats
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_D16_UNORM:
          aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
          break;

        // Depth and stencil formats
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
        case VK_FORMAT_D16_UNORM_S8_UINT:
          aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
          break;

        // Stencil-only format
        case VK_FORMAT_S8_UINT:
          aspectFlag = VK_IMAGE_ASPECT_STENCIL_BIT;
          break;

        // Color formats (or fallback)
        default:
          aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
          break;
      }

      if (cubemap) {
        // Build an image-view for the image
        VkImageViewCreateInfo view_info
            = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
        view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;  // View type for cubemap
        view_info.subresourceRange.levelCount = img_info.mipLevels;
        view_info.subresourceRange.layerCount = 6;  // Layer count for cubemap

        VK_CHECK(vkCreateImageView(gpu_->getDevice(), &view_info, nullptr, &newImage.imageView));

        return newImage;
      }

      // build a image-view for the image
      VkImageViewCreateInfo view_info
          = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
      view_info.subresourceRange.levelCount = img_info.mipLevels;

      VK_CHECK(vkCreateImageView(gpu_->getDevice(), &view_info, nullptr, &newImage.imageView));

      return newImage;
    }

    TextureHandleOld ResourceManager::create_image(void* data, VkExtent3D size, VkFormat format,
                                                 VkImageUsageFlags usage, bool mipmapped) {

      TextureHandleOld new_image = create_image(
          size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
          mipmapped);
      const size_t data_size = static_cast<size_t>(size.depth * size.width * size.height) * 4;
      resource_loader_->addImageTask(new_image, data, data_size, size, mipmapped);

      return new_image;
    }

  void ResourceManager::create_3D_image(const std::shared_ptr<TextureHandleOld>& image,
                                        const VkExtent3D size, const VkFormat format,
                                        const VkImageUsageFlags usage, const std::string& name) const {
      image->setFormat(format);
    image->imageExtent = size;

      VkImageCreateInfo imageCreateInfo{};
      imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
      imageCreateInfo.imageType = VK_IMAGE_TYPE_3D;
      imageCreateInfo.format = format;               
      imageCreateInfo.extent = size;
      imageCreateInfo.mipLevels = 1;
      imageCreateInfo.arrayLayers = 1;
      imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
      imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;  // Use optimal tiling for GPU access
      imageCreateInfo.usage
          = usage | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
      imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; 

      
      // always allocate images on dedicated GPU memory
      VmaAllocationCreateInfo allocinfo = {};
      allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
      allocinfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

      // allocate and create the image
      VK_CHECK(vmaCreateImage(gpu_->getAllocator(), &imageCreateInfo, &allocinfo, &image->image,
                              &image->allocation, nullptr));
      SetDebugName(name, VK_OBJECT_TYPE_IMAGE, gpu_->getDevice(),
                   reinterpret_cast<uint64_t>(image->image));

      VkImageViewCreateInfo viewCreateInfo{};
      viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      viewCreateInfo.image = image->image;
      viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;      
      viewCreateInfo.format = format;                   // Same format as the image
      viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      viewCreateInfo.subresourceRange.baseMipLevel = 0;
      viewCreateInfo.subresourceRange.levelCount = 1;
      viewCreateInfo.subresourceRange.baseArrayLayer = 0;
      viewCreateInfo.subresourceRange.layerCount = 1;

       VK_CHECK(vkCreateImageView(gpu_->getDevice(), &viewCreateInfo, nullptr, &image->imageView));


    }

    TextureHandleOld ResourceManager::create_cubemap(void* imageData, VkExtent3D size,
                                                   VkFormat format, VkImageUsageFlags usage,
                                                   bool mipmapped) {

      // Create the image with VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT flag for cubemaps
      TextureHandleOld new_image = create_image(
          size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
          mipmapped, true);

      resource_loader_->addCubemapTask(new_image, imageData, size);
      return new_image;
    }

    std::optional<TextureHandleOld> ResourceManager::load_image(const std::string& filepath) {
      TextureHandleOld new_image;

      int width, height, nrChannels;
      unsigned char* data = stbi_load(filepath.c_str(), &width, &height, &nrChannels, 4);
      if (data) {
        VkExtent3D imageSize;
        imageSize.width = width;
        imageSize.height = height;
        imageSize.depth = 1;

        new_image = create_image(data, imageSize, VK_FORMAT_R8G8B8A8_UNORM,
                                 VK_IMAGE_USAGE_SAMPLED_BIT, true);

        stbi_image_free(data);
      } else {
        return {};
      }

      if (new_image.image == VK_NULL_HANDLE) {
        return {};
      }
      return new_image;
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

    void ResourceManager::load_and_create_cubemap(const std::string& file_path,
                                                  TextureHandleOld& cubemap) {
      int w, h, comp;
      float* img = stbi_loadf(file_path.c_str(), &w, &h, &comp, 3);
      if (!img) {
        fmt::print("Failed to load image: {}\n", file_path);
        fflush(stdout);
      }

      std::vector<float> img32(w * h * 4);
      float24to32(w, h, img, img32.data());  // Convert HDR format as needed

      cubemap = create_cubemap_from_HDR(img32, h,
                                        w);  // Assume this function initializes cubemap correctly
      stbi_image_free(img);
    }

    void ResourceManager::load_and_process_cubemap(const std::string& file_path) {
      size_t lastDotIndex = file_path.find_last_of('.');
      if (lastDotIndex == std::string::npos) {
        fmt::print("Failed to find file extension in file path: {}\n", file_path);
        return;
      }
      std::string base_path = file_path.substr(0, lastDotIndex);
      std::string extension = file_path.substr(lastDotIndex);  // Includes the dot

      std::string env_file = base_path + "_environment" + extension;
      std::string irr_file = base_path + "_irradiance" + extension;

      auto& Ibl_buffers = repository_->material_buffers;

      if (std::filesystem::exists(env_file) && std::filesystem::exists(irr_file)) {
        fmt::print("Loading environment map from file: {}\n", env_file);
        load_and_create_cubemap(env_file, Ibl_buffers->environment_map);

        fmt::print("Loading irradiance map from file: {}\n", irr_file);
        load_and_create_cubemap(irr_file, Ibl_buffers->environment_irradiance_map);
      } else {
        fmt::print("Loading HDR image from file: {}\n", file_path);
        int w, h, comp;
        float* original_img = stbi_loadf(file_path.c_str(), &w, &h, &comp, 3);
        if (!original_img) {
          fmt::print("Failed to load image: {}\n", file_path);
          fflush(stdout);
          return;
        }

        // Process the HDR image for the environment map
        const int envDstW = 1024;
        const int envDstH = 512;
        std::vector<float> env_out(envDstW * envDstH * 3);  // Using float directly for simplicity
        downsample_equirectangular_map((glm::vec3*)original_img, w, h, envDstW, envDstH,
                                       (glm::vec3*)env_out.data());
        stbi_write_hdr(env_file.c_str(), envDstW, envDstH, 3, env_out.data());

        std::vector<float> img32_env(envDstW * envDstH * 4);
        float24to32(envDstW, envDstH, env_out.data(), img32_env.data());
        Ibl_buffers->environment_map = create_cubemap_from_HDR(img32_env, envDstH, envDstW);

        // Process the HDR image for the irradiance map
        const int irrDstW = 256;
        const int irrDstH = 128;
        std::vector<float> irr_out(irrDstW * irrDstH * 3);  // Adjusted vector type
        convolveDiffuse((glm::vec3*)original_img, w, h, irrDstW, irrDstH,
                        (glm::vec3*)irr_out.data(), 1024);
        stbi_write_hdr(irr_file.c_str(), irrDstW, irrDstH, 3, irr_out.data());

        std::vector<float> img32_irr(irrDstW * irrDstH * 4);
        float24to32(irrDstW, irrDstH, irr_out.data(), img32_irr.data());
        Ibl_buffers->environment_irradiance_map
            = create_cubemap_from_HDR(img32_irr, irrDstH, irrDstW);

        // Cleanup
        stbi_image_free(original_img);
      }

      // Update the sampler and descriptor set for both maps

      VkSamplerCreateInfo sampl
          = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
      sampl.maxLod = VK_LOD_CLAMP_NONE;
      sampl.minLod = 0;
      sampl.magFilter = VK_FILTER_LINEAR;
      sampl.minFilter = VK_FILTER_LINEAR;
      sampl.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
      vkCreateSampler(gpu_->getDevice(), &sampl, nullptr, &Ibl_buffers->cube_map_sampler);

      Ibl_buffers->bdrf_lut = load_image("../../assets/bdrf_lut.png").value();

      resource_loader_->flush();
      vkDeviceWaitIdle(gpu_->getDevice());

     Ibl_buffers->descriptor_buffer->
                write_image(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                             {Ibl_buffers->cube_map_sampler, Ibl_buffers->environment_map.imageView,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL})
                .write_image(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                             {Ibl_buffers->cube_map_sampler,
                              Ibl_buffers->environment_irradiance_map.imageView,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL})
                .write_image(
                    2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    {repository_->get_sampler({.magFilter = VK_FILTER_NEAREST, .minFilter = VK_FILTER_NEAREST, .anisotropyEnable = VK_FALSE}), Ibl_buffers->bdrf_lut.imageView,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL})
                .update();
    }

    TextureHandleOld ResourceManager::create_cubemap_from_HDR(std::vector<float>& image_data, int h,
                                                            int w) {
      Bitmap in(w, h, 4, eBitmapFormat_Float, image_data.data());
      Bitmap out_bitmap = convertEquirectangularMapToVerticalCross(in);

      Bitmap cube = convertVerticalCrossToCubeMapFaces(out_bitmap);

      return create_cubemap(cube.data_.data(),
                            {static_cast<uint32_t>(cube.w_), static_cast<uint32_t>(cube.h_), 1},
                            VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, false);
    }

    void ResourceManager::create_frame_buffer(const std::shared_ptr<TextureHandleOld>& image,
                                              const std::string& name, VkFormat format) const {
      assert(image->getType() == TextureType::kColor || image->getType() == TextureType::kDepth);

      VkImageUsageFlags usageFlags = 0;
      VkImageAspectFlags aspectFlags;

      if (image->getType() == TextureType::kColor) {
        if (format == VK_FORMAT_UNDEFINED) {
          format = VK_FORMAT_R16G16B16A16_SFLOAT;
        }
        if (format == VK_FORMAT_R16G16B16_SFLOAT) {
          fmt::println("Warning: Using R16G16B16_SFLOAT format for color attachment. Switching to VK_FORMAT_R16G16B16A16_SFLOAT for better compatibility.");
          format = VK_FORMAT_R16G16B16A16_SFLOAT;
        }

        usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        usageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
        usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
        aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
      } else if (image->getType() == TextureType::kDepth) {
        if (format == VK_FORMAT_UNDEFINED) {
          format = VK_FORMAT_D32_SFLOAT;
        }
        usageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        usageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
        aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
      }

      image->setFormat(format);
      const VkExtent3D extent = {image->imageExtent.width, image->imageExtent.height, 1};
      image->imageExtent = extent;

      VkImageCreateInfo img_info = vkinit::image_create_info(format, usageFlags, extent);

      // Allocate and create the image
      VmaAllocationCreateInfo img_allocinfo = {};
      img_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
      img_allocinfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

      VK_CHECK(vmaCreateImage(gpu_->getAllocator(), &img_info, &img_allocinfo, &image->image,
                              &image->allocation, nullptr));
      SetDebugName(name, VK_OBJECT_TYPE_IMAGE, gpu_->getDevice(),
                   reinterpret_cast<uint64_t>(image->image));

      // Build an image view for the image to use for rendering
      VkImageViewCreateInfo view_info
          = vkinit::imageview_create_info(format, image->image, aspectFlags);
      VK_CHECK(vkCreateImageView(gpu_->getDevice(), &view_info, nullptr, &image->imageView));
    }


    void ResourceManager::destroy_image(const TextureHandleOld& img) {
      vkDestroyImageView(gpu_->getDevice(), img.imageView, nullptr);
      vmaDestroyImage(gpu_->getAllocator(), img.image, img.allocation);
    }
}  // namespace gestalt