#include "BasicResourceLoader.hpp"

#include "VulkanCheck.hpp"
#include "Interface/IGpu.hpp"
#include "Utils/vk_images.hpp"


namespace gestalt::graphics {
  void BasicResourceLoader::init(IGpu* gpu) {
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

  void BasicResourceLoader::addImageTask(TextureHandleOld image, void* imageData,
                                            VkDeviceSize imageSize, VkExtent3D imageExtent,
                                            bool mipmap) {
    ImageTask task;
    task.image = std::make_shared<TextureHandleOld>(image);
    task.dataCopy = new unsigned char[imageSize];
    memcpy(task.dataCopy, imageData, imageSize);
    task.imageSize = imageSize;
    task.imageExtent = imageExtent;
    task.mipmap = mipmap;
    task.stagingBuffer = {};

    tasks_.push(task);
  }

  void BasicResourceLoader::execute_task(ImageTask& task) {
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

  void BasicResourceLoader::execute_cubemap_task(CubemapTask& task) {
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

  void BasicResourceLoader::addCubemapTask(TextureHandleOld image, void* imageData,
                                              VkExtent3D imageExtent) {
    size_t faceWidth = imageExtent.width;
    size_t faceHeight = imageExtent.height;
    size_t numChannels = 4;  // Assuming RGBA
    size_t bytesPerFloat = sizeof(float);

    size_t faceSizeBytes = faceWidth * faceHeight * numChannels * bytesPerFloat;
    size_t totalCubemapSizeBytes = faceSizeBytes * 6;

    CubemapTask task;
    task.image = std::make_shared<TextureHandleOld>(image);
    task.dataCopy = new unsigned char[totalCubemapSizeBytes];
    memcpy(task.dataCopy, imageData, totalCubemapSizeBytes);
    task.totalCubemapSizeBytes = totalCubemapSizeBytes;
    task.faceSizeBytes = faceSizeBytes;
    task.imageExtent = imageExtent;
    task.stagingBuffer = {};

    cubemap_tasks_.push(task);
  }

  void BasicResourceLoader::add_stagging_buffer(size_t size, AllocatedBufferOld& staging_buffer) {
    // allocate buffer
    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    bufferInfo.size = size;

    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    // allocate the buffer
    VK_CHECK(vmaCreateBuffer(gpu_->getAllocator(), &bufferInfo, &vmaallocInfo,
                             &staging_buffer.buffer, &staging_buffer.allocation,
                             &staging_buffer.info));
  }

  void BasicResourceLoader::flush() {
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

  void BasicResourceLoader::cleanup() const {
    vkFreeCommandBuffers(gpu_->getDevice(), transferCommandPool, 1, &cmd);
    vkDestroyCommandPool(gpu_->getDevice(), transferCommandPool, nullptr);
    vkDestroyFence(gpu_->getDevice(), flushFence, nullptr);
  }
}  // namespace gestalt