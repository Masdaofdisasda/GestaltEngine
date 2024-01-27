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
  virtual void init(vk_gpu& gpu, resource_manager& resource_manager, frame_buffer& frame_buffer) = 0;

  virtual void cleanup(vk_gpu& gpu) = 0;

  virtual void bind_resources() = 0;

  // Execute the rendering logic
  virtual void execute(VkCommandBuffer cmd, sdl_window& window) = 0;
};

class skybox_pass final : public render_pass {
  public:
  void init(vk_gpu& gpu, resource_manager& resource_manager, frame_buffer& frame_buffer) override {
    gpu_ = gpu;
    resource_manager_ = resource_manager;
    frame_buffer_ = frame_buffer;

    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
    };

    descriptor_allocator_.init(gpu.device, 3, sizes);

    {
      descriptor_layout_
          = descriptor_layout_builder()
                .add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                .add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                             VK_SHADER_STAGE_FRAGMENT_BIT)
                .build(gpu.device);
    }

    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pNext = nullptr;
    computeLayout.pSetLayouts = &descriptor_layout_;
    computeLayout.setLayoutCount = 1;

    VK_CHECK(vkCreatePipelineLayout(gpu.device, &computeLayout, nullptr, &pipeline_layout_));

    VkShaderModule vertex_shader;
    vkutil::load_shader_module("../shaders/skybox.vert.spv", gpu.device, &vertex_shader);
    VkShaderModule fragment_shader;
    vkutil::load_shader_module("../shaders/skybox.frag.spv", gpu.device, &fragment_shader);

    pipeline_ = PipelineBuilder()
                    .set_shaders(vertex_shader, fragment_shader)
                    .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                    .set_polygon_mode(VK_POLYGON_MODE_FILL)
                    .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                    .set_multisampling_none()
                    .disable_blending()
                    .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)

                    .set_color_attachment_format(frame_buffer.color_image.imageFormat)
                    .set_depth_format(frame_buffer.depth_image.imageFormat)

                    .set_pipeline_layout(pipeline_layout_)
                    .build_pipeline(gpu.device);

    // Clean up shader modules after pipeline creation
    vkDestroyShaderModule(gpu.device, vertex_shader, nullptr);
    vkDestroyShaderModule(gpu.device, fragment_shader, nullptr);

    VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
    sampl.maxLod = VK_LOD_CLAMP_NONE;
    sampl.minLod = 0;
    sampl.magFilter = VK_FILTER_LINEAR;
    sampl.minFilter = VK_FILTER_LINEAR;
    sampl.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    vkCreateSampler(gpu_.device, &sampl, nullptr, &cube_map_sampler);

    uint32_t white = 0xFFFFFFFF;    // White
    uint32_t red = 0xFFFF0000;      // Red
    uint32_t green = 0xFF00FF00;    // Green
    uint32_t blue = 0xFF0000FF;     // Blue
    uint32_t yellow = 0xFFFFFF00;   // Yellow
    uint32_t magenta = 0xFFFF00FF;  // Magenta

    // Prepare array with one color for each face of the cube
    std::array<void*, 6> cube_colors = {&white, &red, &green, &blue, &yellow, &magenta};

    cube_map_image = resource_manager_.create_cubemap(
        cube_colors, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
  }

  void cleanup(vk_gpu& gpu) override {
      descriptor_allocator_.destroy_pools(gpu.device);

      vkDestroyPipelineLayout(gpu.device, pipeline_layout_, nullptr);
      vkDestroyPipeline(gpu.device, pipeline_, nullptr);

      vkDestroyDescriptorSetLayout(gpu.device, descriptor_layout_, nullptr);
  }

  
  void bind_resources() override {
      cubemap_descriptor_set_ = descriptor_allocator_.allocate(gpu_.device, descriptor_layout_);

      writer.clear();
      writer.write_image(1, cube_map_image.imageView, cube_map_sampler, VK_IMAGE_LAYOUT_GENERAL,
                         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  }

  void execute(const VkCommandBuffer cmd, sdl_window& window) override {

      writer.update_set(gpu_.device, cubemap_descriptor_set_);
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
                              &cubemap_descriptor_set_, 0, nullptr);
      VkViewport viewport = {};
      viewport.x = 0;
      viewport.y = 0;
      viewport.width = static_cast<float>(window.extent.width);
      viewport.height = static_cast<float>(window.extent.height);
      viewport.minDepth = 0.f;
      viewport.maxDepth = 1.f;

      vkCmdSetViewport(cmd, 0, 1, &viewport);

      VkRect2D scissor = {};
      scissor.offset.x = 0;
      scissor.offset.y = 0;
      scissor.extent.width = window.extent.width;
      scissor.extent.height = window.extent.height;

      vkCmdSetScissor(cmd, 0, 1, &scissor);
      vkCmdDraw(cmd, 36, 1, 0, 0);  // 36 vertices for the cube
    
  }

  descriptor_writer writer;

private:
  vk_gpu gpu_ = {};
  resource_manager resource_manager_;
  frame_buffer frame_buffer_ = {};

  VkSampler cube_map_sampler;
  AllocatedImage cube_map_image;

  VkPipeline pipeline_;
  VkPipelineLayout pipeline_layout_;
  VkDescriptorSetLayout descriptor_layout_;
  VkDescriptorSet cubemap_descriptor_set_;
  DescriptorAllocatorGrowable descriptor_allocator_;
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
  frame_buffer frame_buffer_;
  gltf_metallic_roughness gltf_material;

  draw_context main_draw_context_;

  engine_stats stats_;

  gpu_scene_data scene_data;
  AllocatedBuffer gpu_scene_data_buffer;

  void init(const vk_gpu& gpu, const sdl_window& window, resource_manager& resource_manager, const bool& resize_requested,
            engine_stats stats
      ) {

    gpu_ = gpu;
    window_ = window;
    resource_manager_ = resource_manager;
    resize_requested_ = resize_requested;
    stats_ = stats;

    swapchain.init(gpu_, window_, frame_buffer_);
    commands.init(gpu_, frames_);
    sync.init(gpu_, frames_);
    descriptor_manager.init(gpu_, frames_);
    pipeline_manager.init(gpu_, descriptor_manager, gltf_material, frame_buffer_);

    gpu_scene_data_buffer = resource_manager_.create_buffer(
        sizeof(gpu_scene_data), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    skybox_pass_.init(gpu_, resource_manager_, frame_buffer_);
  }

  void draw(imgui_gui& imgui);
  void draw_main(VkCommandBuffer cmd);
  void draw_geometry(VkCommandBuffer cmd);

  void cleanup() {
    for (auto frame : frames_) {
      frame.deletion_queue.flush();
    }

    resource_manager_.destroy_buffer(gpu_scene_data_buffer);

    pipeline_manager.cleanup();
    descriptor_manager.cleanup();
    sync.cleanup();
    commands.cleanup();
    swapchain.destroy_swapchain();
  }
};
