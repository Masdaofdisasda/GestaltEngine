#include "vk_gpu.h"

#include <VkBootstrap.h>

void vk_gpu::init(bool use_validation_layers, sdl_window& window) {
  // create the vulkan instance
  vkb::InstanceBuilder builder;

  // make the vulkan instance, with basic debug features
  auto inst_ret = builder.set_app_name("Gestalt Application")
                      .request_validation_layers(use_validation_layers)
                      .use_default_debug_messenger()
                      .require_api_version(1, 3, 0)
                      .build();

  vkb::Instance vkb_inst = inst_ret.value();

  // grab the instance
  instance = vkb_inst.instance;
  debug_messenger = vkb_inst.debug_messenger;

  // create the device
  window.create_surface(instance, &surface);

  // vulkan 1.3 features
  VkPhysicalDeviceVulkan13Features features{};
  features.dynamicRendering = true;
  features.synchronization2 = true;

  // vulkan 1.2 features
  VkPhysicalDeviceVulkan12Features features12{};
  features12.bufferDeviceAddress = true;
  features12.descriptorIndexing = true;

  // use vkbootstrap to select a gpu.
  // We want a gpu that can write to the SDL surface and supports vulkan 1.3 with the correct
  // features
  vkb::PhysicalDeviceSelector selector{vkb_inst};
  vkb::PhysicalDevice physicalDevice = selector.set_minimum_version(1, 3)
                                           .set_required_features_13(features)
                                           .set_required_features_12(features12)
                                           .set_surface(surface)
                                           .select()
                                           .value();

  // create the final vulkan device
  vkb::DeviceBuilder deviceBuilder{physicalDevice};

  vkb::Device vkbDevice = deviceBuilder.build().value();

  // Get the VkDevice handle used in the rest of a vulkan application
  device = vkbDevice.device;
  chosen_gpu = physicalDevice.physical_device;

  // use vkbootstrap to get a Graphics queue
  graphics_queue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
  graphics_queue_family = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

  // initialize the memory allocator
  VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.physicalDevice = chosen_gpu;
  allocatorInfo.device = device;
  allocatorInfo.instance = instance;
  allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  vmaCreateAllocator(&allocatorInfo, &allocator);
}

void vk_gpu::cleanup() {
  vmaDestroyAllocator(allocator);
  vkDestroyDevice(device, nullptr);
  vkDestroySurfaceKHR(instance, surface, nullptr);
  vkb::destroy_debug_utils_messenger(instance, debug_messenger);
  vkDestroyInstance(instance, nullptr);
}
