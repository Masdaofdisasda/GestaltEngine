#pragma once

#include "imgui_gui.h"
#include "resource_manager.h"
#include "vk_types.h"
#include "vk_gpu.h"
#include "scene_manager.h"

#include "vk_pipelines.h"
#include "vk_swapchain.h"
#include "vk_sync.h"

class vk_renderer;

class render_pass {
public:
  virtual ~render_pass() = default;

  virtual void init(vk_renderer& renderer) = 0;
  virtual void cleanup() = 0;
  virtual void bind_resources() = 0;
  virtual void execute(VkCommandBuffer cmd) = 0;
protected:
  vk_gpu gpu_ = {};
  vk_renderer* renderer_ = nullptr;

  VkPipeline pipeline_;
  VkPipelineLayout pipeline_layout_;
  VkDescriptorSetLayout descriptor_layout_;
  VkDescriptorSet descriptor_set_;
  DescriptorAllocatorGrowable descriptor_allocator_;
};

class skybox_pass final : public render_pass {
public:
  void init(vk_renderer& renderer) override;
  void cleanup() override;
  void bind_resources() override;
  void execute(VkCommandBuffer cmd) override;

  descriptor_writer writer;

private:
  AllocatedImage cube_map_image;
  VkSampler cube_map_sampler;

  std::string vertex_shader_source_ = "../shaders/skybox.vert.spv";
  std::string fragment_shader_source_ = "../shaders/skybox.frag.spv";
};

class pbr_pass final : public render_pass {
public:
  void init(vk_renderer& renderer) override;
  void cleanup() override;
  void bind_resources() override;
  void execute(VkCommandBuffer cmd) override;

  descriptor_writer writer;

private:

  VkPipeline opaquePipeline;//TODO: split into opaque and transparent render passes
  VkPipelineLayout opaquePipelineLayout;
  VkPipeline transparentPipeline;
  VkPipelineLayout transparentPipelineLayout;

  std::string vertex_shader_source_ = "../shaders/mesh.vert.spv";
  std::string fragment_shader_source_ = "../shaders/mesh.frag.spv";
};

constexpr unsigned int FRAME_OVERLAP = 2;

class vk_renderer {
  bool resize_requested_{false}; // TODO

  skybox_pass skybox_pass_ = {};
  pbr_pass pbr_pass_ = {};

public:
  vk_gpu gpu_ = {};
  sdl_window window_;
  std::shared_ptr<resource_manager> resource_manager_;
  std::shared_ptr<scene_manager> scene_manager_;
  std::shared_ptr<imgui_gui> imgui_;

  vk_swapchain swapchain;
  vk_command commands;
  vk_sync sync;

  int frame_number_{0};
  std::vector<frame_data> frames_{FRAME_OVERLAP};
  frame_data& get_current_frame() { return frames_[frame_number_ % FRAME_OVERLAP]; }

  // draw resources
  frame_buffer frame_buffer_;

  draw_context main_draw_context_;

  engine_stats stats_;

  gpu_scene_data scene_data;
  AllocatedBuffer gpu_scene_data_buffer;

  void init(const vk_gpu& gpu, const sdl_window& window,
            const std::shared_ptr<resource_manager>& resource_manager,
            const std::shared_ptr<scene_manager>& scene_manager,
            const std::shared_ptr<imgui_gui>& imgui_gui, const bool& resize_requested,
            engine_stats stats);

  void draw();

  void cleanup() {
    resource_manager_->destroy_buffer(gpu_scene_data_buffer);

    sync.cleanup();
    commands.cleanup();
    swapchain.destroy_swapchain();
  }
};
