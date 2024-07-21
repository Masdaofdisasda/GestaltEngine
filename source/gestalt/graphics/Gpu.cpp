#include "Gpu.hpp"

#include <VkBootstrap.h>

#include "vk_initializers.hpp"

namespace gestalt::graphics {

  void Gpu::init(Window* window) {

    // create the vulkan instance
    vkb::InstanceBuilder builder;

    // make the vulkan instance, with basic debug features
    auto inst_ret = builder.set_app_name("Gestalt Application")
                        .request_validation_layers(useValidationLayers())
                        .enable_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)
                        .use_default_debug_messenger()
                        .require_api_version(1, 3, 0)
                        .build();

    if (!inst_ret.has_value()) {
      throw std::runtime_error("Could not create a Vulkan instance: " + inst_ret.error().message());
    }

    vkb::Instance vkb_inst = inst_ret.value();

    // grab the instance
    instance = vkb_inst.instance;
    debug_messenger = vkb_inst.debug_messenger;

    // create the device
    window->create_surface(instance, &surface);

    VkPhysicalDeviceFeatures features10{};
    features10.fillModeNonSolid = VK_TRUE;
    features10.shaderInt16 = VK_TRUE;

    VkPhysicalDeviceVulkan11Features features11{};
    features11.uniformAndStorageBuffer16BitAccess = VK_TRUE;
    features11.storageBuffer16BitAccess = VK_TRUE;
    features11.shaderDrawParameters = VK_TRUE;

    VkPhysicalDeviceVulkan12Features features12{};
    features12.bufferDeviceAddress = VK_TRUE;
    features12.descriptorIndexing = VK_TRUE;
    features12.uniformAndStorageBuffer8BitAccess = VK_TRUE;
    features12.storageBuffer8BitAccess = VK_TRUE;
    features12.shaderFloat16 = VK_TRUE;
    features12.shaderInt8 = VK_TRUE;
    features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    features12.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
    features12.descriptorBindingVariableDescriptorCount = VK_TRUE;
    features12.runtimeDescriptorArray = VK_TRUE;

    VkPhysicalDeviceVulkan13Features features13{};
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;
    features13.maintenance4 = VK_TRUE;

    VkPhysicalDeviceMeshShaderFeaturesEXT mesh_shader_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
        .taskShader = VK_TRUE,
        .meshShader = VK_TRUE,
        .multiviewMeshShader = VK_FALSE,
        .primitiveFragmentShadingRateMeshShader = VK_TRUE,
        .meshShaderQueries = VK_FALSE};

    VkPhysicalDeviceFragmentShadingRateFeaturesKHR shading_rate_features{};
    shading_rate_features.sType
        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR;
    shading_rate_features.primitiveFragmentShadingRate = VK_TRUE;

    // Link the structures together
    mesh_shader_features.pNext = &shading_rate_features;

    std::vector extensions = {VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
                              VK_EXT_MESH_SHADER_EXTENSION_NAME};

    vkb::PhysicalDeviceSelector selector{vkb_inst};
    auto device_ret = selector.set_minimum_version(1, 3)
                          .set_required_features(features10)
                          .set_required_features_11(features11)
                          .set_required_features_12(features12)
                          .set_required_features_13(features13)
                          .add_required_extension_features(mesh_shader_features)
                          .add_required_extensions(extensions)
                          .set_surface(surface)
                          .select();

    if (!device_ret.has_value()) {
      throw std::runtime_error("Could not select a Vulkan device: " + device_ret.error().message());
    }

    vkb::PhysicalDevice physicalDevice = device_ret.value();

    // create the final vulkan device
    vkb::DeviceBuilder deviceBuilder{physicalDevice};

    auto dev_ret = deviceBuilder.build();

    if (!dev_ret.has_value()) {
      throw std::runtime_error("Could not create a Vulkan device: " + dev_ret.error().message());
    }

    vkb::Device vkbDevice = dev_ret.value();

    // Get the VkDevice handle used in the rest of a vulkan application
    device = vkbDevice.device;
    chosen_gpu = physicalDevice.physical_device;
    vkGetPhysicalDeviceProperties(chosen_gpu, &device_properties);

    // get a Graphics queue
    auto graphics_queue_ret = vkbDevice.get_queue(vkb::QueueType::graphics);

    if (!graphics_queue_ret.has_value()) {
      throw std::runtime_error("Could not get a graphics queue: "
                               + graphics_queue_ret.error().message());
    }
    graphics_queue = graphics_queue_ret.value();
    graphics_queue_family = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    // initialize the memory allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = chosen_gpu;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &allocator);

    vkCmdDrawMeshTasksIndirectCountEXT = reinterpret_cast<PFN_vkCmdDrawMeshTasksIndirectCountEXT>(
        vkGetDeviceProcAddr(device, "vkCmdDrawMeshTasksIndirectCountEXT"));
    if (!vkCmdDrawMeshTasksIndirectCountEXT) {
      throw std::runtime_error("Failed to load vkCmdDrawMeshTasksIndirectCountEXT");
    }

    vkCmdDrawMeshTasksIndirectEXT = reinterpret_cast<PFN_vkCmdDrawMeshTasksIndirectEXT>(
        vkGetDeviceProcAddr(device, "vkCmdDrawMeshTasksIndirectEXT"));
    if (!vkCmdDrawMeshTasksIndirectEXT) {
           throw std::runtime_error("Failed to load vkCmdDrawMeshTasksIndirectEXT");
    }

    // create immediate submit resources
    VkFenceCreateInfo fence_create_info = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VK_CHECK(vkCreateFence(getDevice(), &fence_create_info, nullptr, &immediate_submit_fence_));

    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(
        getGraphicsQueueFamily(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    VK_CHECK(vkCreateCommandPool(getDevice(), &commandPoolInfo, nullptr,
                                 &immediate_submit_command_pool_));
    VkCommandBufferAllocateInfo cmdAllocInfo
        = vkinit::command_buffer_allocate_info(immediate_submit_command_pool_, 1);
    VK_CHECK(vkAllocateCommandBuffers(getDevice(), &cmdAllocInfo, &immediate_submit_command_buffer_));
  }

  void Gpu::immediateSubmit(const std::function<void(VkCommandBuffer cmd)> function) const {
    VK_CHECK(vkResetFences(getDevice(), 1, &immediate_submit_fence_));
    VK_CHECK(vkResetCommandBuffer(immediate_submit_command_buffer_, 0));

    const VkCommandBuffer cmd = immediate_submit_command_buffer_;

    VkCommandBufferBeginInfo cmdBeginInfo
        = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmd_info = vkinit::command_buffer_submit_info(cmd);
    const VkSubmitInfo2 submit = vkinit::submit_info(&cmd_info, nullptr, nullptr);

    VK_CHECK(vkQueueSubmit2(getGraphicsQueue(), 1, &submit, immediate_submit_fence_));
    VK_CHECK(vkWaitForFences(getDevice(), 1, &immediate_submit_fence_, VK_TRUE, UINT64_MAX));
  }

  void Gpu::drawMeshTasksIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer,
      VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset,
      uint32_t maxDrawCount, uint32_t stride) {
    vkCmdDrawMeshTasksIndirectCountEXT(commandBuffer, buffer, offset, countBuffer,
                                       countBufferOffset, maxDrawCount, stride);
  }

  void Gpu::drawMeshTasksIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer,
      VkDeviceSize offset, uint32_t drawCount, uint32_t stride) const {
     vkCmdDrawMeshTasksIndirectEXT(commandBuffer, buffer, offset, drawCount, stride);
  }

  void Gpu::cleanup() const {
    vmaDestroyAllocator(allocator);
    vkDestroyCommandPool(device, immediate_submit_command_pool_, nullptr);
    vkDestroyFence( device, immediate_submit_fence_, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkb::destroy_debug_utils_messenger(instance, debug_messenger);
    vkDestroyInstance(instance, nullptr);
  }

  Gpu::~Gpu() = default;

  VkInstance Gpu::getInstance() const { return instance; }

  VkDevice Gpu::getDevice() const { return device; }

  VmaAllocator Gpu::getAllocator() const { return allocator; }

  VkQueue Gpu::getGraphicsQueue() const { return graphics_queue; }

  uint32_t Gpu::getGraphicsQueueFamily() const { return graphics_queue_family; }

  VkSurfaceKHR Gpu::getSurface() const { return surface; }

  VkDebugUtilsMessengerEXT Gpu::getDebugMessenger() const { return debug_messenger; }

  VkPhysicalDevice Gpu::getPhysicalDevice() const { return chosen_gpu; }

  VkPhysicalDeviceProperties Gpu::getPhysicalDeviceProperties() const { return device_properties; }
}  // namespace gestalt::graphics