#pragma once
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "Gui.h"
#include "RenderConfig.h"
#include "Repository.h"
#include "vk_pipelines.h"
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
      uint32_t version = 0;
    };

    enum class ImageUsageType {
      kRead,
      kWrite,
      kDepthStencilRead,  // used for depth testing
      kCombined           // used for both read and write
    };

    enum class ImageClearOperation {
      kClear,
      kDontCare,
    };

    struct ImageDependency {
      ImageAttachment attachment;
      ImageUsageType usage;
      ImageClearOperation clear_operation;
      uint32_t required_version = 0;
    };

    struct RenderPassDependency {
      std::vector<ShaderProgram> shaders;
      std::vector<ImageDependency> image_attachments;
      VkPushConstantRange push_constant_range;
    };

    class RenderPassDependencyBuilder {
      RenderPassDependency dependency_{};

    public:
      RenderPassDependencyBuilder& add_shader(const ShaderStage stage,
                                              const std::string& source_path) {
        dependency_.shaders.push_back({stage, source_path});
        return *this;
      }
      RenderPassDependencyBuilder& add_image_attachment(const ImageAttachment& attachment,
                                                        const ImageUsageType usage,
                                                        const uint32_t required_version = 0,
                                                        const ImageClearOperation clear_operation
                                                        = ImageClearOperation::kDontCare) {
        dependency_.image_attachments.push_back(
            {attachment, usage, clear_operation, required_version});
        return *this;
      }
      RenderPassDependencyBuilder& set_push_constant_range(uint32_t size,
                                                           VkShaderStageFlags stage_flags) {
        dependency_.push_constant_range = {
            .stageFlags = stage_flags,
            .offset = 0,
            .size = size,
        };
        return *this;
      }
      RenderPassDependency build() { return dependency_; }
    };

    class ResourceRegistry {
    public:
      void init(const Gpu& gpu);

      VkShaderModule get_shader(const ShaderProgram& shader_program);
      void clear_shader_cache();

      RenderConfig config_;

      struct RenderPassAttachments {
        ImageAttachment scene_color{.image = std::make_shared<TextureHandle>(TextureType::kColor)};
        ImageAttachment final_color{.image = std::make_shared<TextureHandle>(TextureType::kColor)};
        ImageAttachment scene_depth{.image = std::make_shared<TextureHandle>(TextureType::kDepth)};
        ImageAttachment shadow_map{.image = std::make_shared<TextureHandle>(TextureType::kDepth),
                                   .extent = {2048, 2048}};
        ImageAttachment gbuffer1{.image = std::make_shared<TextureHandle>(TextureType::kColor)};
        ImageAttachment gbuffer2{.image = std::make_shared<TextureHandle>(TextureType::kColor)};
        ImageAttachment gbuffer3{.image = std::make_shared<TextureHandle>(TextureType::kColor)};

        ImageAttachment bright_pass{.image = std::make_shared<TextureHandle>(TextureType::kColor),
                                    .extent = {512, 512}};
        ImageAttachment blur_y{.image = std::make_shared<TextureHandle>(TextureType::kColor),
                                    .extent = {512, 512}};

        ImageAttachment lum_64{.image = std::make_shared<TextureHandle>(TextureType::kColor),
                               .extent = {64, 64}};
        ImageAttachment lum_32{.image = std::make_shared<TextureHandle>(TextureType::kColor),
                               .extent = {32, 32}};
        ImageAttachment lum_16{.image = std::make_shared<TextureHandle>(TextureType::kColor),
                               .extent = {16, 16}};
        ImageAttachment lum_8{.image = std::make_shared<TextureHandle>(TextureType::kColor),
                              .extent = {8, 8}};
        ImageAttachment lum_4{.image = std::make_shared<TextureHandle>(TextureType::kColor),
                              .extent = {4, 4}};
        ImageAttachment lum_2{.image = std::make_shared<TextureHandle>(TextureType::kColor),
                              .extent = {2, 2}};
        ImageAttachment lum_1{.image = std::make_shared<TextureHandle>(TextureType::kColor),
                              .extent = {1, 1}};

        ImageAttachment lum_A{.image = std::make_shared<TextureHandle>(TextureType::kColor),
                              .extent = {1, 1}};
        ImageAttachment lum_B{.image = std::make_shared<TextureHandle>(TextureType::kColor),
                              .extent = {1, 1}};
      } attachments_;

      std::vector<ImageAttachment> attachment_list_;

    private:
      Gpu gpu_ = {};
      std::unordered_map<std::string, VkShaderModule> shader_cache_{};
    };

    class RenderPass {
    public:
      RenderPass() = default;
      void init(const Gpu& gpu, const std::shared_ptr<ResourceManager>& resource_manager,
                const std::shared_ptr<ResourceRegistry>& registry,
                const std::shared_ptr<Repository>& repository) {
        gpu_ = gpu;
        resource_manager_ = resource_manager;
        registry_ = registry;
        repository_ = repository;

        prepare();
      }
      RenderPassDependency& get_dependencies() { return dependencies_; }

      RenderPass(const RenderPass&) = delete;
      RenderPass& operator=(const RenderPass&) = delete;
      RenderPass(RenderPass&&) = delete;
      RenderPass& operator=(RenderPass&&) = delete;
      virtual ~RenderPass() = default;

      virtual void execute(VkCommandBuffer cmd) = 0;
      virtual std::string get_name() const = 0;
      virtual void cleanup();

    protected:
      virtual void prepare() = 0;
      virtual void destroy() = 0;

      void begin_renderpass(VkCommandBuffer cmd);

      PipelineBuilder create_pipeline();
      void create_pipeline_layout();

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
      std::shared_ptr<Repository> repository_;
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

    class FrameGraphWIP {
    public:
      explicit FrameGraphWIP(const std::vector<std::shared_ptr<RenderPass>>& render_passes);

      void AddEdge(const std::shared_ptr<RenderPass>& u, const std::shared_ptr<RenderPass>& v);

      // Topological Sort using Kahn's Algorithm
      void topologicalSort();

      [[nodiscard]] std::vector<std::shared_ptr<RenderPass>> get_sorted_passes() const;

    private:
      struct ImageWriter {
        std::shared_ptr<RenderPass> pass;
        uint32_t version;
      };

      std::unordered_map<std::shared_ptr<TextureHandle>, ImageWriter> writer_map;
      std::unordered_map<std::shared_ptr<RenderPass>,
                         std::unordered_set<std::shared_ptr<RenderPass>>>
          adj_;
      std::unordered_map<std::shared_ptr<RenderPass>, int> in_degree_;
      std::vector<std::shared_ptr<RenderPass>> sorted_passes_;
    };

    constexpr unsigned char kFrameOverlap = 2;

    class FrameGraph {
      Gpu gpu_ = {};
      application::Window window_;
      std::shared_ptr<ResourceManager> resource_manager_;
      std::shared_ptr<Repository> repository_;
      std::shared_ptr<application::Gui> imgui_;

      std::shared_ptr<ResourceRegistry> resource_registry_ = std::make_shared<ResourceRegistry>();

      std::shared_ptr<VkSwapchain> swapchain_ = std::make_shared<VkSwapchain>();
      std::unique_ptr<VkCommand> commands_ = std::make_unique<VkCommand>();
      std::unique_ptr<vk_sync> sync_ = std::make_unique<vk_sync>();

      std::vector<std::shared_ptr<RenderPass>> render_passes_;

      std::shared_ptr<TextureHandle> debug_texture_;

      bool resize_requested_{false};
      uint32_t swapchain_image_index_{0};
      std::vector<FrameData> frames_{kFrameOverlap};
      bool acquire_next_image();
      void present(VkCommandBuffer cmd);
      FrameData& get_current_frame() { return frames_[gpu_.get_current_frame()]; }
      application::Window& get_window() { return window_; }
      void create_resources();
      VkCommandBuffer start_draw();
      void execute(const std::shared_ptr<RenderPass>& render_pass, VkCommandBuffer cmd);

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
      std::shared_ptr<TextureHandle> get_debug_image() const { return debug_texture_; }
    };
  }  // namespace graphics
}  // namespace gestalt