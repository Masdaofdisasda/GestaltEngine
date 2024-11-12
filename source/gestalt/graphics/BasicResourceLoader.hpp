#pragma once
#include <queue>

#include "VulkanTypes.hpp"
#include "Repository.hpp"
#include "Interface/IResourceManager.hpp"


namespace gestalt::foundation {
  class IGpu;
}

namespace gestalt::graphics {

    class BasicResourceLoader {
      struct ImageTask {
        AllocatedBuffer stagingBuffer;
        std::shared_ptr<TextureHandle> image;
        unsigned char* dataCopy;
        VkDeviceSize imageSize;
        VkExtent3D imageExtent;
        bool mipmap;
      };

      struct CubemapTask {
        AllocatedBuffer stagingBuffer;
        std::shared_ptr<TextureHandle> image;
        unsigned char* dataCopy;
        VkDeviceSize totalCubemapSizeBytes;
        VkDeviceSize faceSizeBytes;
        VkExtent3D imageExtent;
      };

      IGpu* gpu_ = nullptr;
      VkCommandPool transferCommandPool = {};
      VkCommandBuffer cmd = {};
      VkFence flushFence = {};
      std::queue<ImageTask> tasks_;
      std::queue<CubemapTask> cubemap_tasks_;
      size_t max_tasks_per_batch_ = 5;

    public:
      void init(IGpu* gpu);
      void cleanup() const;

      void addImageTask(TextureHandle image, void* imageData, VkDeviceSize imageSize,
                        VkExtent3D imageExtent, bool mipmap);
      void execute_task(ImageTask& task);
      void execute_cubemap_task(CubemapTask& task);
      void addCubemapTask(TextureHandle image, void* imageData, VkExtent3D imageExtent);
      void add_stagging_buffer(size_t size, AllocatedBuffer& staging_buffer);
      void flush();
    };

}  // namespace gestalt