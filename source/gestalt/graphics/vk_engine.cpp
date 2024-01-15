//> includes
#include "vk_engine.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#if 0
#define VMA_DEBUG_INITIALIZE_ALLOCATIONS 1
#  define VMA_DEBUG_LOG_FORMAT(format, ...)
#define VMA_DEBUG_LOG_FORMAT(format, ...) do { \
    printf((format), __VA_ARGS__); \
    printf("\n"); \
} while(false)
#endif
#define VMA_IMPLEMENTATION
#define VMA_VULKAN_VERSION 1003000
#include <vma/vk_mem_alloc.h>

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

#include <VkBootstrap.h>

#include <glm/gtx/transform.hpp>

#include <vk_initializers.h>
#include <vk_types.h>
#include <vk_images.h>
#include <vk_pipelines.h>

#include <chrono>
#include <thread>

vulkan_engine* loaded_engine = nullptr;

constexpr bool bUseValidationLayers = true;

void vulkan_engine::init() {
  // only one engine initialization is allowed with the application.
  assert(loaded_engine == nullptr);
  loaded_engine = this;

  // We initialize SDL and create a window with it.
  SDL_Init(SDL_INIT_VIDEO);

  SDL_WindowFlags window_flags
      = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

  _window = SDL_CreateWindow("Vulkan Engine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                             static_cast<int>(_windowExtent.width),
                             static_cast<int>(_windowExtent.height), window_flags);

  init_vulkan();
  init_swapchain();
  init_commands();
  init_sync_structures();
  init_descriptors();
  init_pipelines();
  init_default_data();
  init_renderables();
  init_imgui();

  // everything went fine
  _isInitialized = true;

  sceneData.ambientColor = glm::vec4(0.1f);
  sceneData.sunlightColor = glm::vec4(1.f);
  sceneData.sunlightDirection = glm::vec4(0.1, 0.5, 0.1, 10.f);

  for (auto& cam : camera_positioners) {
    auto free_fly_camera_ptr = std::make_unique<free_fly_camera>();
    free_fly_camera_ptr->init(glm::vec3(0, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));
    cam = std::move(free_fly_camera_ptr);
  }
  main_camera.init(*camera_positioners.at(current_camera_positioner_index));

}

void vulkan_engine::init_vulkan() {
  // create the vulkan instance
  vkb::InstanceBuilder builder;

  // make the vulkan instance, with basic debug features
  auto inst_ret = builder.set_app_name("Gestalt Application")
                      .request_validation_layers(bUseValidationLayers)
                      .use_default_debug_messenger()
                      .require_api_version(1, 3, 0)
                      .build();

  vkb::Instance vkb_inst = inst_ret.value();

  // grab the instance
  _instance = vkb_inst.instance;
  _debug_messenger = vkb_inst.debug_messenger;

  // create the device
  SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

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
                                           .set_surface(_surface)
                                           .select()
                                           .value();

  // create the final vulkan device
  vkb::DeviceBuilder deviceBuilder{physicalDevice};

  vkb::Device vkbDevice = deviceBuilder.build().value();

  // Get the VkDevice handle used in the rest of a vulkan application
  _device = vkbDevice.device;
  _chosenGPU = physicalDevice.physical_device;

  // use vkbootstrap to get a Graphics queue
  _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
  _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

  // initialize the memory allocator
  VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.physicalDevice = _chosenGPU;
  allocatorInfo.device = _device;
  allocatorInfo.instance = _instance;
  allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  vmaCreateAllocator(&allocatorInfo, &_allocator);

  main_deletion_queue_.init(_device, _allocator);
  main_deletion_queue_.push(_allocator);
}

void vulkan_engine::create_swapchain(uint32_t width, uint32_t height) {
    vkb::SwapchainBuilder swapchainBuilder{_chosenGPU, _device, _surface};

    _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain
        = swapchainBuilder
              //.use_default_format_selection()
              .set_desired_format(VkSurfaceFormatKHR{
                  .format = _swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
              // use vsync present mode
              .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
              .set_desired_extent(width, height)
              .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
              .build()
              .value();

    _swapchainExtent = vkbSwapchain.extent;
    // store swapchain and its related images
    _swapchain = vkbSwapchain.swapchain;
    _swapchainImages = vkbSwapchain.get_images().value();
    _swapchainImageViews = vkbSwapchain.get_image_views().value();
}

void vulkan_engine::destroy_swapchain() {
    vkDestroySwapchainKHR(_device, _swapchain, nullptr);

    // destroy swapchain resources
    for (auto& _swapchainImageView : _swapchainImageViews) {
      vkDestroyImageView(_device, _swapchainImageView, nullptr);
    }
}

void vulkan_engine::init_swapchain() {
    create_swapchain(_windowExtent.width, _windowExtent.height);

    // draw image size will match the window
    VkExtent3D drawImageExtent = {_windowExtent.width, _windowExtent.height, 1};

    // hardcoding the draw format to 32 bit float
    draw_image_.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    draw_image_.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo rimg_info
        = vkinit::image_create_info(draw_image_.imageFormat, drawImageUsages, drawImageExtent);

    // for the draw image, we want to allocate it from gpu local memory
    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo, &draw_image_.image,
                   &draw_image_.allocation, nullptr);

    // build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(
        draw_image_.imageFormat, draw_image_.image, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkCreateImageView(_device, &rview_info, nullptr, &draw_image_.imageView));

    depth_image_.imageFormat = VK_FORMAT_D32_SFLOAT;
    depth_image_.imageExtent = drawImageExtent;
    VkImageUsageFlags depthImageUsages{};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VkImageCreateInfo dimg_info
        = vkinit::image_create_info(depth_image_.imageFormat, depthImageUsages, drawImageExtent);

    // allocate and create the image
    vmaCreateImage(_allocator, &dimg_info, &rimg_allocinfo, &depth_image_.image,
                   &depth_image_.allocation, nullptr);

    // build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(
        depth_image_.imageFormat, depth_image_.image, VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &depth_image_.imageView));

    // add to deletion queues
    main_deletion_queue_.push(draw_image_.imageView);
    main_deletion_queue_.push(draw_image_.image, draw_image_.allocation);
    main_deletion_queue_.push(depth_image_.imageView);
    main_deletion_queue_.push(depth_image_.image, depth_image_.allocation);
}

void vulkan_engine::resize_swapchain() {
    vkDeviceWaitIdle(_device);

    destroy_swapchain();

    int w, h;
    SDL_GetWindowSize(_window, &w, &h);
    _windowExtent.width = w;
    _windowExtent.height = h;

    create_swapchain(_windowExtent.width, _windowExtent.height);

    resize_requested = false;
}

void vulkan_engine::init_commands() {
    // create a command pool for commands submitted to the graphics queue.
    // we also want the pool to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(
        _graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (auto& frame : frames) {
      VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &frame.command_pool));

      // allocate the default command buffer that we will use for rendering
      VkCommandBufferAllocateInfo cmdAllocInfo
          = vkinit::command_buffer_allocate_info(frame.command_pool, 1);

      VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &frame.main_command_buffer));

      main_deletion_queue_.push(frame.command_pool);
    }

    VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_imguiCommandPool));

    // allocate the command buffer for immediate submits
    VkCommandBufferAllocateInfo cmdAllocInfo
        = vkinit::command_buffer_allocate_info(_imguiCommandPool, 1);

    VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_imguiCommandBuffer));

    main_deletion_queue_.push(_imguiCommandPool);
}

void vulkan_engine::init_sync_structures() {

    // create synchronization structures
    // one fence to control when the gpu has finished rendering the frame,
    // and 2 semaphores to synchronize rendering with swapchain
    // we want the fence to start signalled, so we can wait on it on the first frame
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (auto& frame : frames) {
      VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &frame.render_fence));

      VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr,
                                 &frame.swapchain_semaphore));
      VK_CHECK(
          vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &frame.render_semaphore));

      main_deletion_queue_.push(frame.render_fence);
      main_deletion_queue_.push(frame.render_semaphore);
      main_deletion_queue_.push(frame.swapchain_semaphore);
    }

    VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));

    main_deletion_queue_.push(_immFence);
}

void vulkan_engine::init_descriptors() {
    // create a descriptor pool that will hold 10 sets with 1 image each
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
    };

    globalDescriptorAllocator.init(_device, 10, sizes);

    // make the descriptor set layout for our compute draw
    {
      DescriptorLayoutBuilder builder;
      builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
      _drawImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    {
      DescriptorLayoutBuilder builder;
      builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      _singleImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    {
      DescriptorLayoutBuilder builder;
      builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
      _gpuSceneDataDescriptorLayout
          = builder.build(_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    _drawImageDescriptors = globalDescriptorAllocator.allocate(_device, _drawImageDescriptorLayout);

    {
      DescriptorWriter writer;
      writer.write_image(0, draw_image_.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL,
                         VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

      writer.update_set(_device, _drawImageDescriptors);
    }

    for (int i = 0; i < FRAME_OVERLAP; i++) {
      // create a descriptor pool
      std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
          {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
          {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
          {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
          {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
      };

      frames[i].frame_descriptors = DescriptorAllocatorGrowable{};
      frames[i].frame_descriptors.init(_device, 1000, frame_sizes);


      main_deletion_queue_.push_function([this, i]() {
               frames[i].frame_descriptors.destroy_pools(_device);
      });
    }
}


void vulkan_engine::init_pipelines() {
    //COMPUTE PIPELINES
    init_background_pipelines();

    // GRAPHICS PIPELINES
    metalRoughMaterial.build_pipelines(this);
}

void vulkan_engine::init_background_pipelines() {
    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pNext = nullptr;
    computeLayout.pSetLayouts = &_drawImageDescriptorLayout;
    computeLayout.setLayoutCount = 1;

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(compute_push_constants);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    computeLayout.pPushConstantRanges = &pushConstant;
    computeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr, &_gradientPipelineLayout));

    VkShaderModule gradientShader;
    vkutil::load_shader_module("../shaders/gradient_color.comp.spv", _device, &gradientShader);

    VkShaderModule skyShader;
    vkutil::load_shader_module("../shaders/sky.comp.spv", _device, &skyShader);

    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.pNext = nullptr;
    stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageinfo.module = gradientShader;
    stageinfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = _gradientPipelineLayout;
    computePipelineCreateInfo.stage = stageinfo;

    compute_effect gradient;
    gradient.layout = _gradientPipelineLayout;
    gradient.name = "gradient";
    gradient.data = {};

    // default colors
    gradient.data.data1 = glm::vec4(1, 0, 0, 1);
    gradient.data.data2 = glm::vec4(0, 0, 1, 1);

    VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo,
                                      nullptr, &gradient.pipeline));

    // change the shader module only to create the sky shader
    computePipelineCreateInfo.stage.module = skyShader;

    compute_effect sky;
    sky.layout = _gradientPipelineLayout;
    sky.name = "sky";
    sky.data = {};
    // default sky parameters
    sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

    VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo,
                                      nullptr, &sky.pipeline));

    // add the 2 background effects into the array
    backgroundEffects.push_back(gradient);
    backgroundEffects.push_back(sky);

    // destroy structures properly
    vkDestroyShaderModule(_device, gradientShader, nullptr);
    vkDestroyShaderModule(_device, skyShader, nullptr);

    main_deletion_queue_.push(gradient.pipeline);
    main_deletion_queue_.push(sky.pipeline);
    main_deletion_queue_.push(_gradientPipelineLayout);
}

void vulkan_engine::init_imgui() {
    // 1: create descriptor pool for IMGUI
    //  the size of the pool is very oversize, but it's copied from imgui demo
    //  itself.
    VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
                                         {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));

    // 2: initialize imgui library

    // this initializes the core structures of imgui
    ImGui::CreateContext();

    // this initializes imgui for SDL
    ImGui_ImplSDL2_InitForVulkan(_window);

    // this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = _instance;
    init_info.PhysicalDevice = _chosenGPU;
    init_info.Device = _device;
    init_info.Queue = _graphicsQueue;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;
    init_info.ColorAttachmentFormat = _swapchainImageFormat;

    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info, VK_NULL_HANDLE);

    // execute a gpu command to upload imgui font textures
    immediate_submit([&](VkCommandBuffer cmd) { ImGui_ImplVulkan_CreateFontsTexture(); });

    // clear font textures from cpu data
    ImGui_ImplVulkan_DestroyFontsTexture();

    // add the destroy the imgui created structures
    // NOTE: i think ImGui_ImplVulkan_Shutdown() destroy the imguiPool
    main_deletion_queue_.push_function([this]() { ImGui_ImplVulkan_Shutdown(); });
}

void vulkan_engine::init_default_data() {
    std::array<Vertex, 4> rect_vertices;

    rect_vertices[0].position = {0.5, -0.5, 0};
    rect_vertices[1].position = {0.5, 0.5, 0};
    rect_vertices[2].position = {-0.5, -0.5, 0};
    rect_vertices[3].position = {-0.5, 0.5, 0};

    rect_vertices[0].color = {0, 0, 0, 1};
    rect_vertices[1].color = {0.5, 0.5, 0.5, 1};
    rect_vertices[2].color = {1, 0, 0, 1};
    rect_vertices[3].color = {0, 1, 0, 1};

    rect_vertices[0].uv_x = 1;
    rect_vertices[0].uv_y = 0;
    rect_vertices[1].uv_x = 0;
    rect_vertices[1].uv_y = 0;
    rect_vertices[2].uv_x = 1;
    rect_vertices[2].uv_y = 1;
    rect_vertices[3].uv_x = 0;
    rect_vertices[3].uv_y = 1;

    std::array<uint32_t, 6> rect_indices;

    rect_indices[0] = 0;
    rect_indices[1] = 1;
    rect_indices[2] = 2;

    rect_indices[3] = 2;
    rect_indices[4] = 1;
    rect_indices[5] = 3;

    rectangle = uploadMesh(rect_indices, rect_vertices);

    // 3 default textures, white, grey, black. 1 pixel each
    uint32_t white = 0xFFFFFFFF;
    default_material_.white_image = create_image((void*)&white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
                               VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t grey = 0xAAAAAAFF;
    default_material_.grey_image = create_image((void*)&grey, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
                              VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t black = 0x000000FF;
    default_material_.black_image = create_image((void*)&black, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
                               VK_IMAGE_USAGE_SAMPLED_BIT);

    // checkerboard image
    uint32_t magenta = 0xFF00FFFF;
    constexpr size_t checkerboard_size = 256;
    std::array<uint32_t, checkerboard_size> pixels;  // for 16x16 checkerboard texture
    for (int x = 0; x < 16; x++) {
      for (int y = 0; y < 16; y++) {
        pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
      }
    }
    default_material_.error_checkerboard_image = create_image(pixels.data(), VkExtent3D{16, 16, 1},
                                           VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

    sampl.magFilter = VK_FILTER_NEAREST;
    sampl.minFilter = VK_FILTER_NEAREST;

    vkCreateSampler(_device, &sampl, nullptr, &default_material_.default_sampler_nearest);

    sampl.magFilter = VK_FILTER_LINEAR;
    sampl.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(_device, &sampl, nullptr, &default_material_.default_sampler_linear);
}

void vulkan_engine::init_renderables() {
    std::string structurePath = {R"(..\..\assets\structure.glb)"};
    auto structureFile = loadGltf(this, structurePath);

    assert(structureFile.has_value());

    loadedScenes["structure"] = *structureFile;
}

void vulkan_engine::immediate_submit(std::function<void(VkCommandBuffer cmd)> function) {
    VK_CHECK(vkResetFences(_device, 1, &_immFence));
    VK_CHECK(vkResetCommandBuffer(_imguiCommandBuffer, 0));

    VkCommandBuffer cmd = _imguiCommandBuffer;

    VkCommandBufferBeginInfo cmdBeginInfo
        = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    const auto moved_function = std::move(function);
    moved_function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, nullptr, nullptr);

    // submit command buffer to the queue and execute it.
    //  _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, _immFence));

    VK_CHECK(vkWaitForFences(_device, 1, &_immFence, true, 9999999999));
}

AllocatedBuffer vulkan_engine::create_buffer(size_t allocSize, VkBufferUsageFlags usage,
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
    VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer,
                             &newBuffer.allocation, &newBuffer.info));

    return newBuffer;
}

void vulkan_engine::destroy_buffer(const AllocatedBuffer& buffer) {
    vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}
AllocatedImage vulkan_engine::create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                                          bool mipmapped) {
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
    VK_CHECK(vmaCreateImage(_allocator, &img_info, &allocinfo, &newImage.image,
                            &newImage.allocation, nullptr));

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

    VK_CHECK(vkCreateImageView(_device, &view_info, nullptr, &newImage.imageView));

    return newImage;
}

AllocatedImage vulkan_engine::create_image(void* data, VkExtent3D size, VkFormat format,
                                          VkImageUsageFlags usage, bool mipmapped) {
    size_t data_size = static_cast<size_t>(size.depth * size.width * size.height) * 4;
    AllocatedBuffer uploadbuffer
        = create_buffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    memcpy(uploadbuffer.info.pMappedData, data, data_size);

    AllocatedImage new_image = create_image(
        size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        mipmapped);

    immediate_submit([&](VkCommandBuffer cmd) {
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

void vulkan_engine::destroy_image(const AllocatedImage& img) {
        vkDestroyImageView(_device, img.imageView, nullptr);
        vmaDestroyImage(_allocator, img.image, img.allocation);
}

GPUMeshBuffers vulkan_engine::uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices) {
    //> mesh_create_1
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    GPUMeshBuffers newSurface;

    // create vertex buffer
    newSurface.vertexBuffer
        = create_buffer(vertexBufferSize,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                        VMA_MEMORY_USAGE_GPU_ONLY);

    // find the adress of the vertex buffer
    VkBufferDeviceAddressInfo deviceAdressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = newSurface.vertexBuffer.buffer};
    newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(_device, &deviceAdressInfo);

    // create index buffer
    newSurface.indexBuffer = create_buffer(
        indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    //< mesh_create_1
    //
    //> mesh_create_2
    AllocatedBuffer staging
        = create_buffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                        VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = staging.allocation->GetMappedData();

    // copy vertex buffer
    memcpy(data, vertices.data(), vertexBufferSize);
    // copy index buffer
    memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

    immediate_submit([&](VkCommandBuffer cmd) {
      VkBufferCopy vertexCopy{0};
      vertexCopy.dstOffset = 0;
      vertexCopy.srcOffset = 0;
      vertexCopy.size = vertexBufferSize;

      vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

      VkBufferCopy indexCopy{0};
      indexCopy.dstOffset = 0;
      indexCopy.srcOffset = vertexBufferSize;
      indexCopy.size = indexBufferSize;

      vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
    });

    destroy_buffer(staging);

    return newSurface;
    //< mesh_create_2
}

void vulkan_engine::cleanup() {

    if (_isInitialized) {

      vkDeviceWaitIdle(_device);

      loadedScenes.clear();


      for (frame_data frame : frames) {
        frame.deletion_queue.flush();
      }

      main_deletion_queue_.flush();

      destroy_swapchain();
      vkDestroySurfaceKHR(_instance, _surface, nullptr);
      vkDestroyDevice(_device, nullptr);
      vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
      vkDestroyInstance(_instance, nullptr);
      SDL_DestroyWindow(_window);
    }
}

void vulkan_engine::draw_main(VkCommandBuffer cmd) {

    compute_effect& effect = backgroundEffects[currentBackgroundEffect];

    // bind the background compute pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

    // bind the descriptor set containing the draw image for the compute pipeline
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipelineLayout, 0, 1,
                            &_drawImageDescriptors, 0, nullptr);

    vkCmdPushConstants(cmd, _gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(compute_push_constants), &effect.data);
    // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide
    // by it
    vkCmdDispatch(cmd, std::ceil(_windowExtent.width / 16.0),
                  std::ceil(_windowExtent.height / 16.0), 1);

    // draw the triangle

    VkRenderingAttachmentInfo colorAttachment
        = vkinit::attachment_info(draw_image_.imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
    VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(
        depth_image_.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo
        = vkinit::rendering_info(_windowExtent, &colorAttachment, &depthAttachment);

    vkCmdBeginRendering(cmd, &renderInfo);
    auto start = std::chrono::system_clock::now();
    draw_geometry(cmd);

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    stats.mesh_draw_time = elapsed.count() / 1000.f;

    vkCmdEndRendering(cmd);
}

void vulkan_engine::draw() {
    {
      // wait until the gpu has finished rendering the last frame. Timeout of 1 second
      VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame().render_fence, true, 1000000000));

      get_current_frame().deletion_queue.flush();
      get_current_frame().frame_descriptors.clear_pools(_device);
      // request image from the swapchain
      uint32_t swapchainImageIndex;

      VkResult e = vkAcquireNextImageKHR(_device, _swapchain, 1000000000,
                                         get_current_frame().swapchain_semaphore, nullptr,
                                         &swapchainImageIndex);
      if (e == VK_ERROR_OUT_OF_DATE_KHR) {
        resize_requested = true;
        return;
      }

      VK_CHECK(vkResetFences(_device, 1, &get_current_frame().render_fence));

      // now that we are sure that the commands finished executing, we can safely reset the command
      // buffer to begin recording again.
      VK_CHECK(vkResetCommandBuffer(get_current_frame().main_command_buffer, 0));

      // naming it cmd for shorter writing
      VkCommandBuffer cmd = get_current_frame().main_command_buffer;

      // begin the command buffer recording. We will use this command buffer exactly once, so we
      // want to let vulkan know that
      VkCommandBufferBeginInfo cmdBeginInfo
          = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

      //> draw_first
      VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

      // transition our main draw image into general layout so we can write into it
      // we will overwrite it all so we dont care about what was the older layout
      vkutil::transition_image(cmd, draw_image_.image, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_GENERAL);
      vkutil::transition_image(cmd, depth_image_.image, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

      draw_main(cmd);

      // transtion the draw image and the swapchain image into their correct transfer layouts
      vkutil::transition_image(cmd, draw_image_.image, VK_IMAGE_LAYOUT_GENERAL,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
      vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex],
                               VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

      VkExtent2D extent;
      extent.height = _windowExtent.height;
      extent.width = _windowExtent.width;
      //< draw_first
      //> imgui_draw
      // execute a copy from the draw image into the swapchain
      vkutil::copy_image_to_image(cmd, draw_image_.image, _swapchainImages[swapchainImageIndex],
                                  extent, extent);

      // set swapchain image layout to Attachment Optimal so we can draw it
      vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex],
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

      // draw imgui into the swapchain image
      draw_imgui(cmd, _swapchainImageViews[swapchainImageIndex]);

      // set swapchain image layout to Present so we can draw it
      vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex],
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

      // finalize the command buffer (we can no longer add commands, but it can now be executed)
      VK_CHECK(vkEndCommandBuffer(cmd));

      // prepare the submission to the queue.
      // we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain
      // is ready we will signal the _renderSemaphore, to signal that rendering has finished

      VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

      VkSemaphoreSubmitInfo waitInfo
          = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                          get_current_frame().swapchain_semaphore);
      VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(
          VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame().render_semaphore);

      VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

      // submit command buffer to the queue and execute it.
      //  _renderFence will now block until the graphic commands finish execution
      VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, get_current_frame().render_fence));

      // prepare present
      //  this will put the image we just rendered to into the visible window.
      //  we want to wait on the _renderSemaphore for that,
      //  as its necessary that drawing commands have finished before the image is displayed to the
      //  user
      VkPresentInfoKHR presentInfo = vkinit::present_info();

      presentInfo.pSwapchains = &_swapchain;
      presentInfo.swapchainCount = 1;

      presentInfo.pWaitSemaphores = &get_current_frame().render_semaphore;
      presentInfo.waitSemaphoreCount = 1;

      presentInfo.pImageIndices = &swapchainImageIndex;

      VkResult presentResult = vkQueuePresentKHR(_graphicsQueue, &presentInfo);
      if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
        resize_requested = true;
        return;
      }
      // increase the number of frames drawn
      _frameNumber++;
    }
}

void vulkan_engine::draw_background(VkCommandBuffer cmd) {

    compute_effect& effect = backgroundEffects[currentBackgroundEffect];

    // bind the background compute pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

    // bind the descriptor set containing the draw image for the compute pipeline
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipelineLayout, 0, 1,
                            &_drawImageDescriptors, 0, nullptr);

    vkCmdPushConstants(cmd, _gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(compute_push_constants), &effect.data);
    // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide
    // by it
    vkCmdDispatch(cmd, std::ceil(_drawExtent.width / 16.0), std::ceil(_drawExtent.height / 16.0),
                  1);
}

void vulkan_engine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView) {
    VkRenderingAttachmentInfo colorAttachment
        = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
    VkRenderingInfo renderInfo
        = vkinit::rendering_info(_swapchainExtent, &colorAttachment, nullptr);

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

bool is_visible(const render_object& obj, const glm::mat4& viewproj) {
    std::array<glm::vec3, 8> corners{
        glm::vec3{1, 1, 1},  glm::vec3{1, 1, -1},  glm::vec3{1, -1, 1},  glm::vec3{1, -1, -1},
        glm::vec3{-1, 1, 1}, glm::vec3{-1, 1, -1}, glm::vec3{-1, -1, 1}, glm::vec3{-1, -1, -1},
    };

    glm::mat4 matrix = viewproj * obj.transform;

    glm::vec3 min = {1.5, 1.5, 1.5};
    glm::vec3 max = {-1.5, -1.5, -1.5};

    for (int c = 0; c < 8; c++) {
      // project each corner into clip space
      glm::vec4 v = matrix * glm::vec4(obj.bounds.origin + (corners[c] * obj.bounds.extents), 1.f);

      // perspective correction
      v.x = v.x / v.w;
      v.y = v.y / v.w;
      v.z = v.z / v.w;

      min = glm::min(glm::vec3{v.x, v.y, v.z}, min);
      max = glm::max(glm::vec3{v.x, v.y, v.z}, max);
    }

    // check the clip space box is within the view
    if (min.z > 1.f || max.z < 0.f || min.x > 1.f || max.x < -1.f || min.y > 1.f || max.y < -1.f) {
      return false;
    } else {
      return true;
    }
}

void vulkan_engine::draw_geometry(VkCommandBuffer cmd) {
    // begin clock
    auto start = std::chrono::system_clock::now();

    std::vector<uint32_t> opaque_draws;
    opaque_draws.reserve(mainDrawContext.opaque_surfaces.size());

    for (size_t i = 0; i < mainDrawContext.opaque_surfaces.size(); i++) {
      if (is_visible(mainDrawContext.opaque_surfaces[i], sceneData.viewproj)) {
        opaque_draws.push_back(i);
      }
    }

    // sort the opaque surfaces by material and mesh
    std::ranges::sort(opaque_draws, [&](const auto& iA, const auto& iB) {
      const render_object& A = mainDrawContext.opaque_surfaces[iA];
      const render_object& B = mainDrawContext.opaque_surfaces[iB];
      if (A.material == B.material) {
        return A.index_buffer < B.index_buffer;
      }
      return A.material < B.material;
    });

    // allocate a new uniform buffer for the scene data
    AllocatedBuffer gpuSceneDataBuffer = create_buffer(
        sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    // add it to the deletion queue of this frame so it gets deleted once its been used
    get_current_frame().deletion_queue.push_function(
        [gpuSceneDataBuffer, this]() { destroy_buffer(gpuSceneDataBuffer); });

    // write the buffer
    GPUSceneData* sceneUniformData
        = (GPUSceneData*)gpuSceneDataBuffer.allocation->GetMappedData();
    *sceneUniformData = sceneData;

    // create a descriptor set that binds that buffer and update it
    VkDescriptorSet globalDescriptor
        = get_current_frame().frame_descriptors.allocate(_device, _gpuSceneDataDescriptorLayout);

    DescriptorWriter writer;
    writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0,
                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.update_set(_device, globalDescriptor);

    MaterialPipeline* lastPipeline = nullptr;
    MaterialInstance* lastMaterial = nullptr;
    VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

    auto draw = [&](const render_object& r) {
      if (r.material != lastMaterial) {
        lastMaterial = r.material;
        if (r.material->pipeline != lastPipeline) {
          lastPipeline = r.material->pipeline;
          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->pipeline);
          vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  r.material->pipeline->layout, 0, 1, &globalDescriptor, 0,
                                  nullptr);

          VkViewport viewport = {};
          viewport.x = 0;
          viewport.y = 0;
          viewport.width = (float)_windowExtent.width;
          viewport.height = (float)_windowExtent.height;
          viewport.minDepth = 0.f;
          viewport.maxDepth = 1.f;

          vkCmdSetViewport(cmd, 0, 1, &viewport);

          VkRect2D scissor = {};
          scissor.offset.x = 0;
          scissor.offset.y = 0;
          scissor.extent.width = _windowExtent.width;
          scissor.extent.height = _windowExtent.height;

          vkCmdSetScissor(cmd, 0, 1, &scissor);
        }

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                r.material->pipeline->layout, 1, 1, &r.material->materialSet, 0,
                                nullptr);
      }
      if (r.index_buffer != lastIndexBuffer) {
        lastIndexBuffer = r.index_buffer;
        vkCmdBindIndexBuffer(cmd, r.index_buffer, 0, VK_INDEX_TYPE_UINT32);
      }
      // calculate final mesh matrix
      GPUDrawPushConstants push_constants;
      push_constants.worldMatrix = r.transform;
      push_constants.vertexBuffer = r.vertex_buffer_address;

      vkCmdPushConstants(cmd, r.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                         sizeof(GPUDrawPushConstants), &push_constants);

      
      stats.drawcall_count++;
      stats.triangle_count += r.index_count / 3;
      vkCmdDrawIndexed(cmd, r.index_count, 1, r.first_index, 0, 0);
    };
    
    // reset counters
    stats.drawcall_count = 0;
    stats.triangle_count = 0;

    for (auto& r : opaque_draws) {
      draw(mainDrawContext.opaque_surfaces[r]);
    }

    for (auto& r : mainDrawContext.transparent_surfaces) {
      draw(r);
    }

    auto end = std::chrono::system_clock::now();

    // convert to microseconds (integer), and then come back to miliseconds
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    stats.mesh_draw_time = elapsed.count() / 1000.f;

    // we delete the draw commands now that we processed them
    mainDrawContext.opaque_surfaces.clear();
    mainDrawContext.transparent_surfaces.clear();
}

void vulkan_engine::update_scene() {

    glm::mat4 view = main_camera.get_view_matrix();

    // camera projection
    glm::mat4 projection
        = glm::perspective(glm::radians(70.f),
                           (float)_windowExtent.width / (float)_windowExtent.height, 1.f, 1000.f);

    // invert the Y direction on projection matrix so that we are more similar
    // to opengl and gltf axis
    projection[1][1] *= -1;

    sceneData.view = view;
    sceneData.proj = projection;
    sceneData.viewproj = projection * view;

    loadedScenes["structure"]->Draw(glm::mat4{1.f}, mainDrawContext);
}

void vulkan_engine::run()
{
    //begin clock
    auto start = std::chrono::system_clock::now(); // todo replace with timetracker

    time_tracker.update_timer();

    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
      // Handle events on queue
      while (SDL_PollEvent(&e) != 0) {
        // close the window when user alt-f4s or clicks the X button
        if (e.type == SDL_QUIT) bQuit = true;

        input_manager.handle_event(e, _windowExtent.width, _windowExtent.height);

        main_camera.update(time_tracker.get_delta_time(), input_manager.get_movement());

        if (e.type == SDL_WINDOWEVENT) {
          if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
            freeze_rendering = true;
          }
          if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
            freeze_rendering = false;
          }
        }

        // send SDL event to imgui for handling
        ImGui_ImplSDL2_ProcessEvent(&e);
      }

      if (freeze_rendering) {
        // throttle the speed to avoid the endless spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }

      if (resize_requested) {
        resize_swapchain();
      }

      // imgui new frame
      ImGui_ImplVulkan_NewFrame();
      ImGui_ImplSDL2_NewFrame(_window);
      ImGui::NewFrame();

      if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
          if (ImGui::MenuItem("Exit")) {
            bQuit = true;
          }
          // Add more menu items here if needed
          ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
          if (ImGui::BeginMenu("Add Camera")) {
            if (ImGui::MenuItem("Free Fly Camera")) {
              auto free_fly_camera_ptr = std::make_unique<free_fly_camera>();
              free_fly_camera_ptr->init(glm::vec3(0, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0));
              camera_positioners.push_back(std::move(free_fly_camera_ptr));
            }
            if (ImGui::MenuItem("Orbit Camera")) {
              // Code to add an Orbit Camera
            }
            ImGui::EndMenu();
          }

          if (ImGui::BeginMenu("Add Light Source")) {
            if (ImGui::MenuItem("Point Light")) {
              // Code to add an Orbit Camera
            }
            if (ImGui::MenuItem("Directional Light")) {
              // Code to add an Orbit Camera
            }
            if (ImGui::MenuItem("Spot Light")) {
              // Code to add an Orbit Camera
            }
            ImGui::EndMenu();
          }
          // Add more menu items here if needed
          ImGui::EndMenu();
        }
        // Add other menus like "Edit", "View", etc. here

        ImGui::EndMainMenuBar();
      }

      if (ImGui::Begin("Stats")) {
        ImGui::Text("frametime %f ms", stats.frametime);
        ImGui::Text("draw time %f ms", stats.mesh_draw_time);
        ImGui::Text("update time %f ms", stats.scene_update_time);
        ImGui::Text("triangles %i", stats.triangle_count);
        ImGui::Text("draws %i", stats.drawcall_count);
        ImGui::End();
      }

      if (ImGui::Begin("Light")) {
        ImGui::SliderFloat("Light X", &sceneData.sunlightDirection.x, -10.f, 10.f);
        ImGui::SliderFloat("Light Y", &sceneData.sunlightDirection.y, -10.f, 10.f);
        ImGui::SliderFloat("Light Z", &sceneData.sunlightDirection.z, -10.f, 10.f);
        ImGui::End();
      }

      if (ImGui::Begin("Cameras")) {
        for (size_t i = 0; i < camera_positioners.size(); ++i) {
          std::string name = "Camera " + std::to_string(i);

          if (ImGui::Selectable(name.c_str(), current_camera_positioner_index == i)) {
            current_camera_positioner_index = i;
            main_camera.set_positioner(camera_positioners.at(i).get());
          }

          // If this is the currently selected camera, show input fields for position and rotation
          if (current_camera_positioner_index == i) {
            // Get the current position and orientation
            glm::vec3 position = camera_positioners.at(i)->get_position();
            glm::quat orientation = camera_positioners.at(i)->get_orientation();

            // Convert quaternion to Euler angles for user-friendly rotation input
            glm::vec3 rotation = glm::eulerAngles(orientation);

            // Display input fields for position
            if (ImGui::DragFloat3("Position", &position[0])) {
              camera_positioners.at(i)->init(position, glm::vec3(0.f), glm::vec3(0, 1, 0));
            }
          }
        }
        ImGui::End();
      }



      if (ImGui::Begin("Background")) {

        ImGui::SliderFloat("Render Scale", &renderScale, 0.3f, 1.f);

        compute_effect& selected = backgroundEffects[currentBackgroundEffect];

        ImGui::Text("Selected effect: ", selected.name);

        ImGui::SliderInt("Effect Index", &currentBackgroundEffect, 0, backgroundEffects.size() - 1);

        ImGui::InputFloat4("data1", reinterpret_cast<float*>(&selected.data.data1));
        ImGui::InputFloat4("data2", reinterpret_cast<float*>(&selected.data.data2));
        ImGui::InputFloat4("data3", reinterpret_cast<float*>(&selected.data.data3));
        ImGui::InputFloat4("data4", reinterpret_cast<float*>(&selected.data.data4));

        ImGui::End();
      }

      ImGui::Render();

      update_scene();

      // our draw function
      draw();
    }

    // get clock again, compare with start clock
    auto end = std::chrono::system_clock::now();

    // convert to microseconds (integer), and then come back to miliseconds
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    stats.frametime = elapsed.count() / 1000.f;
}

void GLTFMetallic_Roughness::build_pipelines(vulkan_engine* engine) {
    VkShaderModule meshFragShader;
    vkutil::load_shader_module("../shaders/mesh.frag.spv", engine->get_vk_device(), &meshFragShader);

    VkShaderModule meshVertexShader;
    vkutil::load_shader_module("../shaders/mesh.vert.spv", engine->get_vk_device(), &meshVertexShader);

    VkPushConstantRange matrixRange{};
    matrixRange.offset = 0;
    matrixRange.size = sizeof(GPUDrawPushConstants);
    matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    layoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    layoutBuilder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    materialLayout = layoutBuilder.build(engine->get_vk_device(),
                                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    VkDescriptorSetLayout layouts[] = {engine->get_gpu_scene_data_layout(), materialLayout};

    VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
    mesh_layout_info.setLayoutCount = 2;
    mesh_layout_info.pSetLayouts = layouts;
    mesh_layout_info.pPushConstantRanges = &matrixRange;
    mesh_layout_info.pushConstantRangeCount = 1;

    VkPipelineLayout newLayout;
    VK_CHECK(vkCreatePipelineLayout(engine->get_vk_device(), &mesh_layout_info, nullptr, &newLayout));

    opaquePipeline.layout = newLayout;
    transparentPipeline.layout = newLayout;

    // build the stage-create-info for both vertex and fragment stages. This lets
    // the pipeline know the shader modules per stage
    PipelineBuilder pipelineBuilder;
    opaquePipeline.pipeline = pipelineBuilder
      .set_shaders(meshVertexShader, meshFragShader)
      .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
      .set_polygon_mode(VK_POLYGON_MODE_FILL)
      .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
      .set_multisampling_none()
      .disable_blending()
                                  .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)

      // render format
      .set_color_attachment_format(engine->get_draw_image().imageFormat)
      .set_depth_format(engine->get_depth_image().imageFormat)

      // use the triangle layout we created
      .set_pipeline_layout(newLayout)
      .build_pipeline(engine->get_vk_device());

    // create the transparent variant
    transparentPipeline.pipeline = pipelineBuilder
      .enable_blending_additive()
                                       .enable_depthtest(false, VK_COMPARE_OP_LESS_OR_EQUAL)
      .build_pipeline(engine->get_vk_device());

    vkDestroyShaderModule(engine->get_vk_device(), meshFragShader, nullptr);
    vkDestroyShaderModule(engine->get_vk_device(), meshVertexShader, nullptr);
}

MaterialInstance GLTFMetallic_Roughness::write_material(
    VkDevice device, MaterialPass pass, const MaterialResources& resources,
    DescriptorAllocatorGrowable& descriptorAllocator) {
    MaterialInstance matData;
    matData.passType = pass;
    if (pass == MaterialPass::Transparent) {
      matData.pipeline = &transparentPipeline;
    } else {
      matData.pipeline = &opaquePipeline;
    }

    matData.materialSet = descriptorAllocator.allocate(device, materialLayout);

    writer.clear();
    writer.write_buffer(0, resources.dataBuffer, sizeof(MaterialConstants),
                        resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.write_image(1, resources.colorImage.imageView, resources.colorSampler,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.write_image(2, resources.metalRoughImage.imageView, resources.metalRoughSampler,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    writer.update_set(device, matData.materialSet);

    return matData;
}

void mesh_node::Draw(const glm::mat4& topMatrix, draw_context& ctx) {
    glm::mat4 nodeMatrix = topMatrix * worldTransform;

    for (auto& s : mesh->surfaces) {
      render_object def;
      def.index_count = s.count;
      def.first_index = s.startIndex;
      def.index_buffer = mesh->meshBuffers.indexBuffer.buffer;
      def.material = &s.material->data;
      def.bounds = s.bounds;
      def.transform = nodeMatrix;
      def.vertex_buffer_address = mesh->meshBuffers.vertexBufferAddress;

      if (s.material->data.passType == MaterialPass::Transparent) {
        ctx.transparent_surfaces.push_back(def);
      } else {
        ctx.opaque_surfaces.push_back(def);
      }
    }

    // recurse down
    Node::Draw(topMatrix, ctx);
}