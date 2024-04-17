#pragma once
#include <memory>
#include <string>
#include <vector>

#include "Gui.h"
#include "RenderConfig.h"
#include "Repository.h"
#include "vk_sync.h"

namespace gestalt {
  namespace graphics {

    enum class ShaderStage : uint32_t {
      kVertex = VK_SHADER_STAGE_VERTEX_BIT,
      kTessellationControl = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
      kTessellationEvaluation = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
      kGeometry = VK_SHADER_STAGE_GEOMETRY_BIT,
      kFragment = VK_SHADER_STAGE_FRAGMENT_BIT,
      kCompute = VK_SHADER_STAGE_COMPUTE_BIT,
      kRayGen = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
      kAnyHit = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
      kClosestHit = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
      kMiss = VK_SHADER_STAGE_MISS_BIT_KHR,
      kIntersection = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
      kCallable = VK_SHADER_STAGE_CALLABLE_BIT_KHR,
      kTask = VK_SHADER_STAGE_TASK_BIT_NV,
      kMesh = VK_SHADER_STAGE_MESH_BIT_NV
    };

    struct ShaderProgram {
      ShaderStage stage;
      std::string source_path;
    };

    struct ImageAttachment {
      std::shared_ptr<TextureHandle> image;
      float scale = 1.0f;
      VkExtent3D extent = {0, 0, 1};
    };

    enum class ImageUsageType {
      kRead,
      kWrite,
      kDepthStencilRead,  // used for depth testing
    };

    enum class ImageClearOperation {
      kClear,
      kDontCare,
    };

    struct ImageDependency {
      ImageAttachment attachment;
      ImageUsageType usage;
      ImageClearOperation clear_operation;
    };

    struct RenderPassDependency {
      std::vector<ShaderProgram> shaders;
      std::vector<ImageDependency> image_attachments;
    };

    class RenderPassDependencyBuilder {
      RenderPassDependency dependency_{};
    public:
      RenderPassDependencyBuilder& add_shader(const ShaderStage stage, const std::string& source_path) {
        dependency_.shaders.push_back({stage, source_path});
        return *this;
      }
      RenderPassDependencyBuilder& add_image_attachment(const ImageAttachment& attachment,
                                                        const ImageUsageType usage,
                                                        const ImageClearOperation clear_operation
                                                        = ImageClearOperation::kDontCare) {
        dependency_.image_attachments.push_back({attachment, usage, clear_operation});
        return *this;
      }
      RenderPassDependency build() { return dependency_; }
    };

    class ResourceRegistry {
    public:

       void init(const Gpu& gpu) {
                gpu_ = gpu;
       }

       VkShaderModule get_shader(const ShaderProgram& shader_program);
      void clear_shader_cache();

       RenderConfig config_;

      struct RenderPassAttachments {
        ImageAttachment scene_color{.image = std::make_shared<TextureHandle>(TextureType::kColor)};
        ImageAttachment scene_depth{.image = std::make_shared<TextureHandle>(TextureType::kDepth)};
      } attachments_;

    private:
      Gpu gpu_ = {};
      std::unordered_map<std::string, VkShaderModule> shader_cache_{};
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
      virtual RenderPassDependency& get_dependencies() = 0;
      virtual std::string get_name() const = 0;
      virtual void cleanup() = 0;

    protected:
      virtual void prepare() = 0;

      void begin_renderpass(VkCommandBuffer cmd);

      RenderPassDependency dependencies_{};

      VkViewport viewport_{
          .x = 0,
          .y = 0,
          .minDepth = 0.f,
          .maxDepth = 1.f,
      };
      VkRect2D scissor_{
          .offset = {0, 0},
      };

      VkPipeline pipeline_ = nullptr;
      VkPipelineLayout pipeline_layout_ = nullptr;
      std::vector<VkDescriptorSetLayout> descriptor_layouts_;

      Gpu gpu_ = {};
      std::shared_ptr<ResourceManager> resource_manager_;
      std::shared_ptr<ResourceRegistry> registry_;
      std::shared_ptr<foundation::Repository> repository_;
    };

    class VkSwapchain {
      Gpu gpu_;

    public:
      VkSwapchainKHR swapchain;
      VkFormat swapchain_image_format;
      VkExtent2D swapchain_extent;
      VkExtent2D draw_extent;

      std::vector<std::shared_ptr<foundation::TextureHandle>> swapchain_images;

      void init(const Gpu& gpu, const VkExtent3D& extent);
      void create_swapchain(uint32_t width, uint32_t height);
      void resize_swapchain(application::Window& window);
      void destroy_swapchain() const;
    };

    constexpr unsigned char kFrameOverlap = 2;

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

      bool resize_requested_{false};
      uint32_t swapchain_image_index_{0};
      std::vector<FrameData> frames_{kFrameOverlap};
      bool acquire_next_image();
      void present(VkCommandBuffer cmd);
      FrameData& get_current_frame() { return frames_[gpu_.get_current_frame()]; }
      application::Window& get_window() { return window_; }

      std::string get_final_transformation(const std::string& original);
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
      VkFormat get_swapchain_format() const { return swapchain_->swapchain_image_format; }
    };
  }  // namespace graphics
}  // namespace gestalt