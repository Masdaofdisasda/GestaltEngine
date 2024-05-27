#pragma once

#include <ranges>

class DeletionService {

  VkDevice device_;
  VmaAllocator allocator_;
  std::deque<std::function<void()>> deletors_;

public:

  void init(VkDevice device, VmaAllocator allocator) {
       device_ = device;
    allocator_ = allocator;
  }

  void push_function(std::function<void()> function) {
       deletors_.emplace_back(function);
  }

  void push(VmaAllocator allocator) {
       deletors_.emplace_back([=]() { vmaDestroyAllocator(allocator); });
  }

  void push(VkImageView imageView) {
    deletors_.emplace_back([this, imageView]() {
         if (imageView != VK_NULL_HANDLE) {
           vkDestroyImageView(device_, imageView, nullptr);
         }
    });
  }

  void push(VkImage image, VmaAllocation allocation) {
    deletors_.emplace_back([this, image, allocation]() { vmaDestroyImage(allocator_, image, allocation); });
  }

  void push(VkBuffer buffer, VmaAllocation allocation) {
    deletors_.emplace_back([this, buffer, allocation]() { vmaDestroyBuffer(allocator_, buffer, allocation); });
  }

  void push(VkPipeline pipeline) {
    deletors_.emplace_back([this, pipeline]() {
      if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, pipeline, nullptr);
      }
    });
  }

  void push(VkPipelineLayout pipelineLayout) {
    deletors_.emplace_back([this, pipelineLayout]() {
      if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipelineLayout, nullptr);
      }
    });
  }

  void push(VkRenderPass renderPass) {
    deletors_.emplace_back([this, renderPass]() {
      if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, renderPass, nullptr);
      }
    });
  }

  void push(VkFramebuffer framebuffer) {
    deletors_.emplace_back([this, framebuffer]() {
      if (framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device_, framebuffer, nullptr);
      }
    });
  }

  void push(VkDescriptorSetLayout descriptorSetLayout) {
    deletors_.emplace_back([this, descriptorSetLayout]() {
      if (descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, descriptorSetLayout, nullptr);
      }
    });
  }

  void push(VkSampler sampler) {
    deletors_.emplace_back([this, sampler]() {
      if (sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device_, sampler, nullptr);
      }
    });
  }

  void push(VkShaderModule shaderModule) { vkDestroyShaderModule(device_, shaderModule, nullptr);
  }

  void push(VkCommandPool commandPool) {
       deletors_.emplace_back([this, commandPool]() {
      if (commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, commandPool, nullptr);
      }
       });
  }

  void push(VkFence fence) {
       deletors_.emplace_back([this, fence]() {
         if (fence != VK_NULL_HANDLE) {
           vkDestroyFence(device_, fence, nullptr);
         }
    });
  }

  void push(VkSemaphore semaphore) {
          deletors_.emplace_back([this, semaphore]() {
         if (semaphore != VK_NULL_HANDLE) {
           vkDestroySemaphore(device_, semaphore, nullptr);
         }
          });
  }

  void push(VkDescriptorPool descriptorPool) {
          deletors_.emplace_back([this, descriptorPool]() {
            if (descriptorPool != VK_NULL_HANDLE) {
              vkDestroyDescriptorPool(device_, descriptorPool, nullptr);
            }
          });
  }

  void flush() {
    for (auto& deletor : std::ranges::reverse_view(deletors_)) {
      deletor();
    }
    deletors_.clear();
  }
};