#pragma once
#include <memory>
#include <string>
#include <vector>

#include "imgui_gui.h"
#include "render_pass.h"
#include "vk_swapchain.h"
#include "vk_sync.h"

constexpr unsigned int FRAME_OVERLAP = 2;

class frame_graph {
  vk_gpu gpu_ = {};
  sdl_window window_;
  std::shared_ptr<resource_manager> resource_manager_;
  std::shared_ptr<imgui_gui> imgui_;

  std::shared_ptr<vk_swapchain> swapchain_ = std::make_unique<vk_swapchain>();
  std::unique_ptr<vk_command> commands_ = std::make_unique<vk_command>();
  std::unique_ptr<vk_sync> sync_ = std::make_unique<vk_sync>();

  std::vector<std::unique_ptr<shader_pass>> render_passes_;
  // Maps each shader pass to indices of passes it depends on (those that write resources it reads).
  std::unordered_map<size_t, std::vector<size_t>> graph_;
  // Tracks how many passes each pass depends on.
  std::unordered_map<size_t, size_t> in_degree_;
  std::vector<size_t> sorted_passes_;

  std::unordered_map<std::string, std::string> direct_original_mapping_;
  std::unordered_map<std::string, std::string> resource_transformations_;
  // tracks the inital and final state of each written resource
  std::vector<std::pair<std::string, std::string>> resource_pairs_;
  
  bool resize_requested_{false};
  uint32_t swapchain_image_index_{0};
  uint32_t frame_number_{0};
  std::vector<frame_data> frames_{FRAME_OVERLAP};
  bool acquire_next_image();
  void present(VkCommandBuffer cmd);
  frame_data& get_current_frame() { return frames_[frame_number_ % FRAME_OVERLAP]; }
  sdl_window& get_window() { return window_; }

  void build_graph();
  void populate_graph();
  void topological_sort();
  std::string get_final_transformation(const std::string& original);
  void calculate_resource_transform();
  void create_resources();
  VkCommandBuffer start_draw();
  void execute(size_t id, VkCommandBuffer cmd);

public:

  void init(const vk_gpu& gpu, const sdl_window& window,
            const std::shared_ptr<resource_manager>& resource_manager, const std::shared_ptr<imgui_gui>&
            imgui_gui);

  void execute_passes();

  void cleanup();

  vk_sync& get_sync() const { return *sync_; }
  vk_command& get_commands() const { return *commands_; }
  std::shared_ptr<vk_swapchain> get_swapchain() const { return swapchain_; }
};
