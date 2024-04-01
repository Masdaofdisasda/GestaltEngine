#pragma once
#include <memory>
#include <string>
#include <vector>

#include "Gui.h"
#include "RenderConfig.h"
#include "Repository.h"
#include "vk_swapchain.h"
#include "vk_sync.h"

namespace gestalt {
  namespace graphics {

    class ShaderResource {
    public:
      virtual ~ShaderResource() = default;

      virtual std::string getId() const = 0;
    };

    class ColorImageResource final : public ShaderResource {

      std::string id;
      float scale_ = 1.0f;
      VkExtent2D extent_ = {0, 0};

      std::shared_ptr<foundation::TextureHandle> image_;

    public:
      ColorImageResource(std::string id, VkExtent2D extent) : id(std::move(id)), extent_(extent) {}

      ColorImageResource(std::string id, const float scale) : id(std::move(id)), scale_(scale) {}

      std::string getId() const override { return id; }
      float get_scale() const { return scale_; }
      VkExtent2D get_extent() const { return extent_; }
      void set_image(const std::shared_ptr<foundation::TextureHandle>& image) { image_ = image; }
    };

    class DepthImageResource final : public ShaderResource {

      std::string id;
      float scale_ = 1.0f;
      VkExtent2D extent_ = {0, 0};

      std::shared_ptr<foundation::TextureHandle> image_;

    public:
      DepthImageResource(std::string id, VkExtent2D extent) : id(std::move(id)), extent_(extent) {}
      DepthImageResource(std::string id, const float scale) : id(std::move(id)), scale_(scale) {}

      std::string getId() const override { return id; }
      float get_scale() const { return scale_; }
      VkExtent2D get_extent() const { return extent_; }
      void set_image(const std::shared_ptr<foundation::TextureHandle>& image) { image_ = image; }
    };

    class BufferResource final : public ShaderResource {
      std::string id;
      VkDeviceSize size;

    public:
      BufferResource(const std::string& id, VkDeviceSize size) : id(id), size(size) {}

      std::string getId() const override { return id; }
      VkDeviceSize get_size() const { return size; }
    };

    struct ShaderPassDependencyInfo {
      std::vector<std::shared_ptr<ShaderResource>> read_resources;
      std::vector<std::pair<std::string, std::shared_ptr<ShaderResource>>> write_resources;
    };

    class ResourceRegistry {
    public:
      RenderConfig config_;
      PerFrameData per_frame_data_;

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
          if (const auto original_it = resources_.find(original_id);
              original_it != resources_.end()) {
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
      std::unordered_map<std::string, std::shared_ptr<TextureHandle>> resources_;
    };

    class RenderPass {
    public:
      void init(const Gpu& gpu, const std::shared_ptr<ResourceManager>& resource_manager,
                const std::shared_ptr<ResourceRegistry>& registry,
                const std::shared_ptr<foundation::Repository>& repository) {
        gpu_ = gpu;
        resource_manager_ = resource_manager;
        registry_ = registry;
        repository_ = repository;

        prepare();
      }
      virtual ~RenderPass() = default;

      virtual void execute(VkCommandBuffer cmd) = 0;
      virtual ShaderPassDependencyInfo& get_dependencies() = 0;
      virtual void cleanup() = 0;

    protected:
      virtual void prepare() = 0;

      Gpu gpu_ = {};
      std::shared_ptr<ResourceManager> resource_manager_;
      std::shared_ptr<ResourceRegistry> registry_;
      std::shared_ptr<foundation::Repository> repository_;
    };

    constexpr unsigned int FRAME_OVERLAP = 2;

    class FrameGraph {

      Gpu gpu_ = {};
      application::Window window_;
      std::shared_ptr<ResourceManager> resource_manager_;
      std::shared_ptr<foundation::Repository> repository_;
      std::shared_ptr<application::Gui> imgui_;

      std::shared_ptr<ResourceRegistry> resource_registry_
          = std::make_shared<ResourceRegistry>();

      std::shared_ptr<VkSwapchain> swapchain_ = std::make_unique<VkSwapchain>();
      std::unique_ptr<VkCommand> commands_ = std::make_unique<VkCommand>();
      std::unique_ptr<vk_sync> sync_ = std::make_unique<vk_sync>();

      std::vector<std::unique_ptr<RenderPass>> render_passes_;
      // Maps each shader pass to indices of passes it depends on (those that write resources it
      // reads).
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
      std::vector<FrameData> frames_{FRAME_OVERLAP};
      bool acquire_next_image();
      void present(VkCommandBuffer cmd);
      FrameData& get_current_frame() { return frames_[frame_number_ % FRAME_OVERLAP]; }
      application::Window& get_window() { return window_; }

      void build_graph();
      void populate_graph();
      void topological_sort();
      std::string get_final_transformation(const std::string& original);
      void calculate_resource_transform();
      void create_resources();
      VkCommandBuffer start_draw();
      void execute(size_t id, VkCommandBuffer cmd);

    public:
      void init(const Gpu& gpu, const application::Window& window,
                const std::shared_ptr<ResourceManager>& resource_manager,
                const std::shared_ptr<foundation::Repository>& repository,
                const std::shared_ptr<application::Gui>& imgui_gui);

      void execute_passes();

      void cleanup();

      RenderConfig& get_config() { return resource_registry_->config_; }
      vk_sync& get_sync() const { return *sync_; }
      VkCommand& get_commands() const { return *commands_; }
      std::shared_ptr<VkSwapchain> get_swapchain() const { return swapchain_; }
    };
  }  // namespace graphics
}  // namespace gestalt