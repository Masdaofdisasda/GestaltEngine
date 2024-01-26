#pragma once

#include "imgui_gui.h"
#include "resource_manager.h"
#include "vk_types.h"
#include "vk_gpu.h"
#include "vk_scene_manager.h"

#include "vk_pipelines.h"
#include "vk_swapchain.h"
#include "vk_sync.h"

class render_pass {
public:
  virtual ~render_pass() = default;

  // Initialize the render pass with Vulkan context
  virtual void init(vk_gpu& gpu, AllocatedImage draw_image) = 0;

  virtual void cleanup(vk_gpu& gpu) = 0;

  // Execute the rendering logic
  virtual void execute(VkCommandBuffer cmd, sdl_window& window) = 0;
};

class skybox_pass final : public render_pass {
  public:
  void init(vk_gpu& gpu, AllocatedImage draw_image) override {

    // create a descriptor pool that will hold 10 sets with 1 image each
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
    };

    global_descriptor_allocator.init(gpu.device, 10, sizes);

    // make the descriptor set layout for our compute draw
    {
      DescriptorLayoutBuilder builder;
      draw_image_descriptor_layout = builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                                         .build(gpu.device, VK_SHADER_STAGE_COMPUTE_BIT);

    }

    {
      DescriptorLayoutBuilder builder;
      single_image_descriptor_layout
          = builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                .build(gpu.device, VK_SHADER_STAGE_FRAGMENT_BIT);

    }

    {
      DescriptorLayoutBuilder builder;
      gpu_scene_data_descriptor_layout
          = builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                .build(gpu.device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    }

    draw_image_descriptors
        = global_descriptor_allocator.allocate(gpu.device, draw_image_descriptor_layout);

    {
      DescriptorWriter writer;
      writer.write_image(0, draw_image.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL,
                         VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

      writer.update_set(gpu.device, draw_image_descriptors);
    }


      VkPipelineLayoutCreateInfo computeLayout{};
      computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
      computeLayout.pNext = nullptr;
      computeLayout.pSetLayouts = &draw_image_descriptor_layout;
      computeLayout.setLayoutCount = 1;

      VkPushConstantRange pushConstant{};
      pushConstant.offset = 0;
      pushConstant.size = sizeof(compute_push_constants);
      pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

      computeLayout.pPushConstantRanges = &pushConstant;
      computeLayout.pushConstantRangeCount = 1;

      VK_CHECK(
          vkCreatePipelineLayout(gpu.device, &computeLayout, nullptr, &pipeline_layout));

      VkShaderModule gradient_shader;
      vkutil::load_shader_module("../shaders/gradient_color.comp.spv", gpu.device,
                                 &gradient_shader);

      VkPipelineShaderStageCreateInfo stageinfo{};
      stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stageinfo.pNext = nullptr;
      stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
      stageinfo.module = gradient_shader;
      stageinfo.pName = "main";

      VkComputePipelineCreateInfo computePipelineCreateInfo{};
      computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
      computePipelineCreateInfo.pNext = nullptr;
      computePipelineCreateInfo.layout = pipeline_layout;
      computePipelineCreateInfo.stage = stageinfo;

      sky_gradient_.layout = pipeline_layout;
      sky_gradient_.name = "gradient";
      sky_gradient_.data = {};

      // default colors
      sky_gradient_.data.data1 = glm::vec4(0, 0.3f, 1, 1);
      sky_gradient_.data.data2 = glm::vec4(0, 0.3f, 0, 1);

      VK_CHECK(vkCreateComputePipelines(gpu.device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo,
                                        nullptr, &sky_gradient_.pipeline));

      // destroy structures properly
      vkDestroyShaderModule(gpu.device, gradient_shader, nullptr);
  }

  void cleanup(vk_gpu& gpu) override {
      global_descriptor_allocator.destroy_pools(gpu.device);

      vkDestroyPipelineLayout(gpu.device, pipeline_layout, nullptr);
      vkDestroyPipeline(gpu.device, sky_gradient_.pipeline, nullptr);

      vkDestroyDescriptorSetLayout(gpu.device, draw_image_descriptor_layout, nullptr);
      vkDestroyDescriptorSetLayout(gpu.device, gpu_scene_data_descriptor_layout, nullptr);
      vkDestroyDescriptorSetLayout(gpu.device, single_image_descriptor_layout, nullptr);
  }

  void execute(const VkCommandBuffer cmd, sdl_window& window) override {

    // bind the background compute pipeline
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sky_gradient_.pipeline);

    // bind the descriptor set containing the draw image for the compute pipeline
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipeline_layout, 0, 1,
                            &draw_image_descriptors, 0, nullptr);

    vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(compute_push_constants), &sky_gradient_.data);
    // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide
    // by it
    vkCmdDispatch(cmd, std::ceil(window.extent.width / 16.0),
                  std::ceil(window.extent.height / 16.0), 1);
  }

private:
  VkPipeline gradient_pipeline;
  VkPipelineLayout pipeline_layout;

 compute_effect sky_gradient_;
  DescriptorAllocatorGrowable global_descriptor_allocator;

  VkDescriptorSet draw_image_descriptors;
  VkDescriptorSetLayout draw_image_descriptor_layout;

  VkDescriptorSetLayout single_image_descriptor_layout;
  VkDescriptorSetLayout gpu_scene_data_descriptor_layout;
};

constexpr unsigned int FRAME_OVERLAP = 2;

class vk_renderer {

  vk_gpu gpu_ = {};
  sdl_window window_;
  resource_manager resource_manager_;
  bool resize_requested_{false}; // TODO

  skybox_pass skybox_pass_ = {};

public:
  vk_scene_manager* scene_manager_; //WIP

  vk_swapchain swapchain;
  vk_command commands;
  vk_sync sync;
  vk_descriptor_manager descriptor_manager;
  vk_pipeline_manager pipeline_manager;

  int frame_number_{0};
  std::vector<frame_data> frames_{FRAME_OVERLAP};
  frame_data& get_current_frame() { return frames_[frame_number_ % FRAME_OVERLAP]; }

  // draw resources
  AllocatedImage draw_image_;
  AllocatedImage depth_image_;
  gltf_metallic_roughness gltf_material;

  draw_context main_draw_context_;

  engine_stats stats_;

  gpu_scene_data scene_data;

  void init(const vk_gpu& gpu, const sdl_window& window, resource_manager& resource_manager, const bool& resize_requested,
            engine_stats stats
      ) {

    gpu_ = gpu;
    window_ = window;
    resource_manager_ = resource_manager;
    resize_requested_ = resize_requested;
    stats_ = stats;

    swapchain.init(gpu_, window_, draw_image_, depth_image_);
    commands.init(gpu_, frames_);
    sync.init(gpu_, frames_);
    descriptor_manager.init(gpu_, frames_);
    pipeline_manager.init(gpu_, descriptor_manager, gltf_material,
                          draw_image_, depth_image_);

    skybox_pass_.init(gpu_, draw_image_);
  }

  void draw(imgui_gui& imgui);
  void draw_main(VkCommandBuffer cmd);
  void draw_geometry(VkCommandBuffer cmd);

  void cleanup() {
    for (auto frame : frames_) {
      frame.deletion_queue.flush();
    }

    pipeline_manager.cleanup();
    descriptor_manager.cleanup();
    sync.cleanup();
    commands.cleanup();
    swapchain.destroy_swapchain();
  }
};
