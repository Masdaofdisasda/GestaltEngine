#include "vk_gpu.h"

#include <VkBootstrap.h>

void vk_gpu::init(
    bool use_validation_layers, sdl_window& window,
    std::function<void(std::function<void(VkCommandBuffer)>)> immediate_submit_function) {
  immediate_submit = immediate_submit_function;

  // create the vulkan instance
  vkb::InstanceBuilder builder;

  // make the vulkan instance, with basic debug features
  auto inst_ret = builder.set_app_name("Gestalt Application")
                      .request_validation_layers(use_validation_layers)
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
  window.create_surface(instance, &surface);

  VkPhysicalDeviceFeatures features10{};
  features10.fillModeNonSolid = true;

  VkPhysicalDeviceVulkan11Features features11{};
  features11.uniformAndStorageBuffer16BitAccess = true;
  features11.storageBuffer16BitAccess = true;
  features11.shaderDrawParameters = true;

  VkPhysicalDeviceVulkan12Features features12{};
  features12.bufferDeviceAddress = true;
  features12.descriptorIndexing = true;
  features12.uniformAndStorageBuffer8BitAccess = true;
  features12.shaderFloat16 = true;
  features12.shaderInt8 = true;
  features12.shaderSampledImageArrayNonUniformIndexing = true;
  features12.shaderStorageBufferArrayNonUniformIndexing = true;
  features12.descriptorBindingVariableDescriptorCount = true;
  features12.runtimeDescriptorArray = true;

  VkPhysicalDeviceVulkan13Features features13{};
  features13.dynamicRendering = true;
  features13.synchronization2 = true;

  VkPhysicalDeviceMeshShaderFeaturesNV mesh_shader_features{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV};
  mesh_shader_features.taskShader = true;
  mesh_shader_features.meshShader = true;

  std::vector extensions
      = {VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME, VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
         VK_NV_MESH_SHADER_EXTENSION_NAME, VK_EXT_MESH_SHADER_EXTENSION_NAME};

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

  // use vkbootstrap to get a Graphics queue
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

  vkCmdPushDescriptorSetKHR = reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(
      vkGetDeviceProcAddr(device, "vkCmdPushDescriptorSetKHR"));

  if (!vkCmdPushDescriptorSetKHR) {
    throw std::runtime_error("Failed to load vkCmdPushDescriptorSetKHR");
  }

  vkCmdDrawMeshTasksIndirectCountEXT = reinterpret_cast<PFN_vkCmdDrawMeshTasksIndirectCountEXT>(
      vkGetDeviceProcAddr(device, "vkCmdDrawMeshTasksIndirectCountEXT"));
  if (!vkCmdDrawMeshTasksIndirectCountEXT) {
       throw std::runtime_error("Failed to load vkCmdDrawMeshTasksIndirectCountEXT");
  }
}

void vk_gpu::cleanup() {
  vmaDestroyAllocator(allocator);
  vkDestroyDevice(device, nullptr);
  vkDestroySurfaceKHR(instance, surface, nullptr);
  vkb::destroy_debug_utils_messenger(instance, debug_messenger);
  vkDestroyInstance(instance, nullptr);
}
