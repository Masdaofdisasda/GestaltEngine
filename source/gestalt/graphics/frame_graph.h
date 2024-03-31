#pragma once
#include <memory>
#include <string>
#include <vector>

#include "imgui_gui.h"
#include "vk_swapchain.h"
#include "vk_sync.h"


class shader_resource {
public:
  virtual ~shader_resource() = default;

  virtual std::string getId() const = 0;
};

class color_image_resource final : public shader_resource {
  std::string id;
  float scale_ = 1.0f;
  VkExtent2D extent_ = {0, 0};

  std::shared_ptr<AllocatedImage> image_;

public:
  color_image_resource(std::string id, VkExtent2D extent) : id(std::move(id)), extent_(extent) {}

  color_image_resource(std::string id, const float scale) : id(std::move(id)), scale_(scale) {}

  std::string getId() const override { return id; }
  float get_scale() const { return scale_; }
  VkExtent2D get_extent() const { return extent_; }
  void set_image(const std::shared_ptr<AllocatedImage>& image) { image_ = image; }
};

class depth_image_resource final : public shader_resource {
  std::string id;
  float scale_ = 1.0f;
  VkExtent2D extent_ = {0, 0};

  std::shared_ptr<AllocatedImage> image_;

public:
  depth_image_resource(std::string id, VkExtent2D extent) : id(std::move(id)), extent_(extent) {}
  depth_image_resource(std::string id, const float scale) : id(std::move(id)), scale_(scale) {}

  std::string getId() const override { return id; }
  float get_scale() const { return scale_; }
  VkExtent2D get_extent() const { return extent_; }
  void set_image(const std::shared_ptr<AllocatedImage>& image) { image_ = image; }
};

class buffer_resource final : public shader_resource {
  std::string id;
  VkDeviceSize size;

public:
  buffer_resource(const std::string& id, VkDeviceSize size) : id(id), size(size) {}

  std::string getId() const override { return id; }
  VkDeviceSize get_size() const { return size; }
};

struct shader_pass_dependency_info {
  std::vector<std::shared_ptr<shader_resource>> read_resources;
  std::vector<std::pair<std::string, std::shared_ptr<shader_resource>>> write_resources;
};

class RenderResourceRegistry {
public:
    
  template <typename T> void add_resource(const std::string& id, std::shared_ptr<T> resource) {
    resources_[id] = resource;
  }

  template <typename T> std::shared_ptr<T> get_resource(const std::string& id) {
    if (const auto it = resources_.find(id); it != resources_.end()) {
      return std::dynamic_pointer_cast<T>(it->second);
    }
    if (const auto direct_it = direct_original_mapping.find(id);
        direct_it != direct_original_mapping.end()) {
      const std::string& original_id = direct_it->second;
      if (const auto original_it = resources_.find(original_id); original_it != resources_.end()) {
        return std::dynamic_pointer_cast<T>(original_it->second);
      }
    }
    return nullptr;
  }

  bool update_resource_id(const std::string& oldId, const std::string& newId) {
    auto it = resources_.find(oldId);
    if (it == resources_.end()) {
      return false;
    }
    resources_[newId] = it->second;
    resources_.erase(it);
    return true;
  }

  // TODO replace this with a more general solution
  std::unordered_map<std::string, std::string> direct_original_mapping;

private:
  std::unordered_map<std::string, std::shared_ptr<AllocatedImage>> resources_;

};

class render_pass {
public:
  void init(const vk_gpu& gpu, const std::shared_ptr<resource_manager>& resource_manager, const std::shared_ptr<RenderResourceRegistry>& registry, const std::shared_ptr<Repository>& repository) {
    gpu_ = gpu;
    resource_manager_ = resource_manager;
    registry_ = registry;
    repository_ = repository;

    prepare();
  }
  virtual ~render_pass() = default;

  virtual void execute(VkCommandBuffer cmd) = 0;
  virtual shader_pass_dependency_info& get_dependencies() = 0;
  virtual void cleanup() = 0;

protected:
  virtual void prepare() = 0;

  vk_gpu gpu_ = {};
  std::shared_ptr<resource_manager> resource_manager_;
  std::shared_ptr<RenderResourceRegistry> registry_;
  std::shared_ptr<Repository> repository_;
};

constexpr unsigned int FRAME_OVERLAP = 2;

class frame_graph {
  vk_gpu gpu_ = {};
  sdl_window window_;
  std::shared_ptr<resource_manager> resource_manager_;
  std::shared_ptr<Repository> repository_;
  std::shared_ptr<imgui_gui> imgui_;

  std::shared_ptr<RenderResourceRegistry> resource_registry_ = std::make_shared<RenderResourceRegistry>();

  std::shared_ptr<vk_swapchain> swapchain_ = std::make_unique<vk_swapchain>();
  std::unique_ptr<vk_command> commands_ = std::make_unique<vk_command>();
  std::unique_ptr<vk_sync> sync_ = std::make_unique<vk_sync>();

  std::vector<std::unique_ptr<render_pass>> render_passes_;
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
      const std::shared_ptr<resource_manager>& resource_manager,
      const std::shared_ptr<Repository>& repository, const std::shared_ptr<imgui_gui>&
            imgui_gui);

  void execute_passes();

  void cleanup();

  vk_sync& get_sync() const { return *sync_; }
  vk_command& get_commands() const { return *commands_; }
  std::shared_ptr<vk_swapchain> get_swapchain() const { return swapchain_; }
};
