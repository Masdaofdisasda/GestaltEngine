#include "ResourceManager.hpp"

#include "vk_images.hpp"
#include "vk_initializers.hpp"

#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>

#include "CubemapUtils.hpp"
#include "VulkanCheck.hpp"
#include "Interface/IGpu.hpp"


namespace gestalt::graphics {

    void ResourceManager::init(IGpu* gpu,
                               Repository* repository) {
      gpu_ = gpu;
      repository_ = repository;
      resource_loader_.init(gpu);
      generate_samplers();
    }

    void ResourceManager::cleanup() {
      resource_loader_.cleanup();
    }

    void ResourceManager::flush_loader() { resource_loader_.flush(); }

  void SetDebugName(std::string name, VkObjectType type, VkDevice device, uint64_t handle) {
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

    std::shared_ptr<AllocatedBuffer> ResourceManager::create_buffer(size_t allocSize,
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

      AllocatedBuffer new_buffer;
      new_buffer.usage = usage;
      new_buffer.memory_usage = memoryUsage;

      VK_CHECK(vmaCreateBuffer(gpu_->getAllocator(), &bufferInfo, &vmaallocInfo, &new_buffer.buffer,
                               &new_buffer.allocation, &new_buffer.info));

      SetDebugName( name, VK_OBJECT_TYPE_BUFFER, gpu_->getDevice(), reinterpret_cast<uint64_t>(new_buffer.buffer));

      const VkBufferDeviceAddressInfo device_address_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = new_buffer.buffer};
      new_buffer.address
          = vkGetBufferDeviceAddress(gpu_->getDevice(), &device_address_info);

      return std::make_shared<AllocatedBuffer>(new_buffer);
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

    void ResourceManager::destroy_buffer(const std::shared_ptr<AllocatedBuffer> buffer) {
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

    TextureHandle ResourceManager::create_image(VkExtent3D size, VkFormat format,
                                                 VkImageUsageFlags usage, bool mipmapped,
                                                 bool cubemap) {

      TextureHandle newImage{cubemap ? TextureType::kCubeMap : TextureType::kColor};
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

    TextureHandle ResourceManager::create_image(void* data, VkExtent3D size, VkFormat format,
                                                 VkImageUsageFlags usage, bool mipmapped) {

      TextureHandle new_image = create_image(
          size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
          mipmapped);
      const size_t data_size = static_cast<size_t>(size.depth * size.width * size.height) * 4;
      resource_loader_.addImageTask(new_image, data, data_size, size, mipmapped);

      return new_image;
    }

    TextureHandle ResourceManager::create_cubemap(void* imageData, VkExtent3D size,
                                                   VkFormat format, VkImageUsageFlags usage,
                                                   bool mipmapped) {

      // Create the image with VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT flag for cubemaps
      TextureHandle new_image = create_image(
          size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
          mipmapped, true);

      resource_loader_.addCubemapTask(new_image, imageData, size);
      return new_image;
    }

    std::optional<TextureHandle> ResourceManager::load_image(const std::string& filepath) {
      TextureHandle new_image;

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

    void PoorMansResourceLoader::init(IGpu* gpu) {
	  gpu_ = gpu;
      VkCommandPoolCreateInfo poolInfo = {};
      poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
      poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
      poolInfo.queueFamilyIndex = gpu_->getGraphicsQueueFamily();

      VK_CHECK(vkCreateCommandPool(gpu_->getDevice(), &poolInfo, nullptr, &transferCommandPool));

      VkCommandBufferAllocateInfo allocInfo = {};
      allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      allocInfo.commandPool = transferCommandPool;
      allocInfo.commandBufferCount = 1;

      VK_CHECK(vkAllocateCommandBuffers(gpu_->getDevice(), &allocInfo, &cmd));

      VkFenceCreateInfo fenceInfo = {};
      fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      VK_CHECK(vkCreateFence(gpu_->getDevice(), &fenceInfo, nullptr, &flushFence));
    }

    void PoorMansResourceLoader::addImageTask(TextureHandle image, void* imageData,
                                              VkDeviceSize imageSize, VkExtent3D imageExtent, bool mipmap) {

      ImageTask task;
      task.image = std::make_shared<TextureHandle>(image);
      task.dataCopy = new unsigned char[imageSize];
      memcpy(task.dataCopy, imageData, imageSize);
      task.imageSize = imageSize;
      task.imageExtent = imageExtent;
      task.mipmap = mipmap;
      task.stagingBuffer = {};

      tasks_.push(task);
    }

    void PoorMansResourceLoader::execute_task(ImageTask& task) {

      add_stagging_buffer(task.imageSize, task.stagingBuffer);

      void* data;
      VK_CHECK(vmaMapMemory(gpu_->getAllocator(), task.stagingBuffer.allocation, &data));
      memcpy(data, task.dataCopy, task.imageSize);
      vmaUnmapMemory(gpu_->getAllocator(), task.stagingBuffer.allocation);

      vkutil::TransitionImage(task.image)
          .to(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
          .withSource(VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0)
          .withDestination(VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT)
          .andSubmitTo(cmd);

      VkBufferImageCopy copyRegion = {};
      copyRegion.bufferOffset = 0;
      copyRegion.bufferRowLength = 0;
      copyRegion.bufferImageHeight = 0;

      copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copyRegion.imageSubresource.mipLevel = 0;
      copyRegion.imageSubresource.baseArrayLayer = 0;
      copyRegion.imageSubresource.layerCount = 1;
      copyRegion.imageExtent = task.imageExtent;

      // copy the buffer into the image
      vkCmdCopyBufferToImage(cmd, task.stagingBuffer.buffer, task.image->image,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

      if (task.mipmap) {
        vkutil::generate_mipmaps(cmd, task.image);
      }

      vkutil::TransitionImage(task.image)
          .to(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
          .withSource(VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT)
          .withDestination(VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT)
          .andSubmitTo(cmd);
    }

    void PoorMansResourceLoader::execute_cubemap_task(CubemapTask & task) {

      add_stagging_buffer(task.totalCubemapSizeBytes, task.stagingBuffer);

      // Copy each face data into the buffer
      void* data;
      VK_CHECK(vmaMapMemory(gpu_->getAllocator(), task.stagingBuffer.allocation, &data));
      memcpy(data, task.dataCopy, task.totalCubemapSizeBytes);
      vmaUnmapMemory(gpu_->getAllocator(), task.stagingBuffer.allocation);

      vkutil::TransitionImage(task.image)
          .to(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
          .withSource(VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0)
          .withDestination(VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT)
          .andSubmitTo(cmd);

      // Copy each face from the buffer to the image
      for (int i = 0; i < 6; ++i) {
        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = task.faceSizeBytes * i;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = i;  // Specify the face index
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = task.imageExtent;

        vkCmdCopyBufferToImage(cmd, task.stagingBuffer.buffer, task.image->image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
      }

      if (false) {
        // vkutil::generate_mipmaps( TODO
        // cmd, new_image.image,
        // VkExtent2D{new_image.imageExtent.width, new_image.imageExtent.height});
      } else {
        vkutil::TransitionImage(task.image)
            .to(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .withSource(VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT)
            .withDestination(VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT)
            .andSubmitTo(cmd);
      }
    }

    void PoorMansResourceLoader::addCubemapTask(TextureHandle image, void* imageData,
                                                VkExtent3D imageExtent) {
      size_t faceWidth = imageExtent.width;
      size_t faceHeight = imageExtent.height;
      size_t numChannels = 4;  // Assuming RGBA
      size_t bytesPerFloat = sizeof(float);

      size_t faceSizeBytes = faceWidth * faceHeight * numChannels * bytesPerFloat;
      size_t totalCubemapSizeBytes = faceSizeBytes * 6;

      CubemapTask task;
      task.image = std::make_shared<TextureHandle>(image);
      task.dataCopy = new unsigned char[totalCubemapSizeBytes];
      memcpy(task.dataCopy, imageData, totalCubemapSizeBytes);
      task.totalCubemapSizeBytes = totalCubemapSizeBytes;
      task.faceSizeBytes = faceSizeBytes;
      task.imageExtent = imageExtent;
      task.stagingBuffer = {};

      cubemap_tasks_.push(task);
    }

    void PoorMansResourceLoader::add_stagging_buffer(size_t size, AllocatedBuffer& staging_buffer) {

        // allocate buffer
      VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
      bufferInfo.pNext = nullptr;
      bufferInfo.size = size;

      bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

      VmaAllocationCreateInfo vmaallocInfo = {};
      vmaallocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
      vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

      // allocate the buffer
      VK_CHECK(vmaCreateBuffer(gpu_->getAllocator(), &bufferInfo, &vmaallocInfo, &staging_buffer.buffer,
                               &staging_buffer.allocation, &staging_buffer.info));
    }

    void PoorMansResourceLoader::flush() {
      if (tasks_.empty() && cubemap_tasks_.empty()) {
        return;
      }

      VkCommandBufferBeginInfo beginInfo = {};
      beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

      size_t tasksProcessed = 0;
      std::queue<ImageTask> tasksDone;
      std::queue<CubemapTask> cubemapTasksDone;
      while (!tasks_.empty() && tasksProcessed < max_tasks_per_batch_) {
        ImageTask& task = tasks_.front();
        execute_task(task);  // Execute the task
        tasksDone.push(task);
        tasks_.pop();
        tasksProcessed++;
      }

      while (!cubemap_tasks_.empty() && tasksProcessed < max_tasks_per_batch_) {
        CubemapTask& task = cubemap_tasks_.front();
        execute_cubemap_task(task);  // Execute the cubemap task
        cubemapTasksDone.push(task);
        cubemap_tasks_.pop();
        tasksProcessed++;
      }

      VK_CHECK(vkEndCommandBuffer(cmd));

      VkCommandBufferSubmitInfo commandBufferSubmitInfo = {};
      commandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
      commandBufferSubmitInfo.commandBuffer = cmd;

      VkSubmitInfo2 submitInfo2 = {};
      submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
      submitInfo2.commandBufferInfoCount = 1;
      submitInfo2.pCommandBufferInfos = &commandBufferSubmitInfo;

      VK_CHECK(vkQueueSubmit2(gpu_->getGraphicsQueue(), 1, &submitInfo2, flushFence));
      VK_CHECK(vkWaitForFences(gpu_->getDevice(), 1, &flushFence, VK_TRUE, UINT64_MAX));
      VK_CHECK(vkResetFences(gpu_->getDevice(), 1, &flushFence));
      VK_CHECK(vkResetCommandBuffer(cmd, 0));

      while (!tasksDone.empty()) {
        ImageTask& task = tasksDone.front();
        auto& stagingBuffer = task.stagingBuffer;
        vmaDestroyBuffer(gpu_->getAllocator(), stagingBuffer.buffer, stagingBuffer.allocation);

        delete[] task.dataCopy;
        tasksDone.pop();
      }
      while (!cubemapTasksDone.empty()) {
        CubemapTask& task = cubemapTasksDone.front();
        auto& stagingBuffer = task.stagingBuffer;
        vmaDestroyBuffer(gpu_->getAllocator(), stagingBuffer.buffer, stagingBuffer.allocation);

        delete[] task.dataCopy;
        cubemapTasksDone.pop();
      }
    }

    void PoorMansResourceLoader::cleanup() {
      vkFreeCommandBuffers(gpu_->getDevice(), transferCommandPool, 1, &cmd);
	  vkDestroyCommandPool(gpu_->getDevice(), transferCommandPool, nullptr);
	  vkDestroyFence(gpu_->getDevice(), flushFence, nullptr);
    }

    void ResourceManager::load_and_create_cubemap(const std::string& file_path,
                                                  TextureHandle& cubemap) {
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

      resource_loader_.flush();
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

    TextureHandle ResourceManager::create_cubemap_from_HDR(std::vector<float>& image_data, int h,
                                                            int w) {
      Bitmap in(w, h, 4, eBitmapFormat_Float, image_data.data());
      Bitmap out_bitmap = convertEquirectangularMapToVerticalCross(in);

      Bitmap cube = convertVerticalCrossToCubeMapFaces(out_bitmap);

      return create_cubemap(cube.data_.data(),
                            {static_cast<uint32_t>(cube.w_), static_cast<uint32_t>(cube.h_), 1},
                            VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, false);
    }

    void ResourceManager::create_frame_buffer(const std::shared_ptr<TextureHandle>& image, VkFormat format) const {
      assert(image->getType() == TextureType::kColor || image->getType() == TextureType::kDepth);

      VkImageUsageFlags usageFlags = 0;
      VkImageAspectFlags aspectFlags;

      if (image->getType() == TextureType::kColor) {
        if (format == VK_FORMAT_UNDEFINED) {
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

      // Build an image view for the image to use for rendering
      VkImageViewCreateInfo view_info
          = vkinit::imageview_create_info(format, image->image, aspectFlags);
      VK_CHECK(vkCreateImageView(gpu_->getDevice(), &view_info, nullptr, &image->imageView));
    }


    void ResourceManager::destroy_image(const TextureHandle& img) {
      vkDestroyImageView(gpu_->getDevice(), img.imageView, nullptr);
      vmaDestroyImage(gpu_->getAllocator(), img.image, img.allocation);
    }
}  // namespace gestalt