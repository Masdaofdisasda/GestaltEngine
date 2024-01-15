#pragma once

class deletion_queue {

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
    deletors_.emplace_back([=]() { vkDestroyImageView(device_, imageView, nullptr); });
  }

  void push(VkImage image, VmaAllocation allocation) {
    deletors_.emplace_back([=]() { vmaDestroyImage(allocator_, image, allocation); });
  }

  void push(VkBuffer buffer, VmaAllocation allocation) {
    deletors_.emplace_back([=]() { vmaDestroyBuffer(allocator_, buffer, allocation); });
  }

  void push(VkPipeline pipeline) {
    deletors_.emplace_back([=]() { vkDestroyPipeline(device_, pipeline, nullptr); });
  }

  void push(VkPipelineLayout pipelineLayout) {
    deletors_.emplace_back([=]() { vkDestroyPipelineLayout(device_, pipelineLayout, nullptr); });
  }

  void push(VkRenderPass renderPass) {
    deletors_.emplace_back([=]() { vkDestroyRenderPass(device_, renderPass, nullptr); });
  }

  void push(VkFramebuffer framebuffer) {
    deletors_.emplace_back([=]() { vkDestroyFramebuffer(device_, framebuffer, nullptr); });
  }

  void push(VkDescriptorSetLayout descriptorSetLayout) {
    deletors_.emplace_back(
        [=]() { vkDestroyDescriptorSetLayout(device_, descriptorSetLayout, nullptr); });
  }

  void push(VkSampler sampler) {
    deletors_.emplace_back([=]() { vkDestroySampler(device_, sampler, nullptr); });
  }

  void push(VkShaderModule shaderModule) {
    deletors_.emplace_back([=]() { vkDestroyShaderModule(device_, shaderModule, nullptr); });
  }

  void push(VkCommandPool commandPool) {
       deletors_.emplace_back([=]() { vkDestroyCommandPool(device_, commandPool, nullptr); });
  }

  void push(VkFence fence) {
       deletors_.emplace_back([=]() { vkDestroyFence(device_, fence, nullptr); });
  }

  void push(DescriptorAllocatorGrowable descriptorAllocator) {
    // review this
       deletors_.emplace_back([=]() mutable { descriptorAllocator.destroy_pools(device_); });
  }

  void push(VkDescriptorPool descriptorPool) {
          deletors_.emplace_back([=]() { vkDestroyDescriptorPool(device_, descriptorPool, nullptr); });
  }

  void flush() {
    for (auto it = deletors_.rbegin(); it != deletors_.rend(); ++it) {
      (*it)();
    }
    deletors_.clear();
  }
};