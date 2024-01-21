#pragma once

#include "imgui_gui.h"
#include "resource_manager.h"
#include "vk_types.h"
#include "vk_gpu.h"
#include "vk_loader.h"

#include "vk_pipelines.h"
#include "vk_swapchain.h"
#include "vk_sync.h"

constexpr unsigned int FRAME_OVERLAP = 2;

class vk_render_system {

  vk_gpu gpu_;
  sdl_window window_;
  resource_manager resource_manager_;
  bool resize_requested_{false}; // TODO

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
  gltf_metallic_roughness metal_rough_material_;
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
    descriptor_manager.init(gpu_, frames_, draw_image_);
    pipeline_manager.init(gpu_, descriptor_manager, metal_rough_material_, gltf_material,
                          draw_image_, depth_image_);
  }

  void draw(imgui_gui& imgui);
  void draw_main(VkCommandBuffer cmd);
  void draw_geometry(VkCommandBuffer cmd);

  void cleanup() {
    descriptor_manager.cleanup();
    sync.cleanup();
    commands.cleanup();
    swapchain.destroy_swapchain();
  }
};
