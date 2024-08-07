#pragma once
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "Gui.hpp"
#include "RenderConfig.hpp"
#include "Repository.hpp"
#include "ResourceManager.hpp"
#include "vk_pipelines.hpp"

namespace gestalt::graphics {

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
      float32 scale{1.0f};
      VkExtent3D extent{0, 0, 1};
      VkFormat format{VK_FORMAT_UNDEFINED};
      VkClearValue initial_value{};
      uint32 version{0};
    };

    enum class ImageUsageType {
      kRead,               // will only be read as input to the shader
      kWrite,              // will only be written as attachment by the shader
      kDepthStencilRead,  // used only for depth testing
      kCombined           // will be used for read or write operations (eg. ping-pong passes)
    };

    enum class ImageClearOperation {
      kClear,     // clear the image before rendering
      kDontCare,  // don't clear the image
    };

    struct ImageDependency {
      ImageAttachment attachment;
      ImageUsageType usage;
      ImageClearOperation clear_operation;
      uint32 required_version = 0;
    };

  struct BufferResource {
      BufferCollection* buffer;
  };

  enum class BufferUsageType {
    kRead,
    kWrite,
  };

  struct BufferDependency {
    BufferResource resource;
    BufferUsageType usage;
    uint32 set; // Descriptor set binding index in GLSL layout

    uint32 required_version = 0;
    };

    struct RenderPassDependency {
      std::vector<ShaderProgram> shaders;
      std::vector<ImageDependency> image_attachments;
      std::vector<BufferDependency> buffer_dependencies;
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
                                                        const uint32 required_version = 0,
                                                        const ImageClearOperation clear_operation
                                                        = ImageClearOperation::kDontCare);

      RenderPassDependencyBuilder& add_buffer_dependency(const BufferResource& buffer,
                                                         const BufferUsageType usage,
                                                         const uint32 set,
                                                         const uint32 required_version = 0);

      RenderPassDependencyBuilder& set_push_constant_range(uint32 size,
                                                           VkShaderStageFlags stage_flags);
      RenderPassDependency build();
    };

    class ResourceRegistry : public NonCopyable<ResourceRegistry> {
    public:
      void init(IGpu* gpu, const Repository* repository);

      VkShaderModule get_shader(const ShaderProgram& shader_program);
      void clear_shader_cache();

      RenderConfig config_;

      struct RenderPassResources {
        ImageAttachment scene_color{.image = std::make_shared<TextureHandle>(TextureType::kColor)};
        ImageAttachment final_color{.image = std::make_shared<TextureHandle>(TextureType::kColor)};
        ImageAttachment scene_depth{.image = std::make_shared<TextureHandle>(TextureType::kDepth)};
        ImageAttachment shadow_map{.image = std::make_shared<TextureHandle>(TextureType::kDepth),
                                   .extent = {2048, 2048}};
        ImageAttachment gbuffer1{.image = std::make_shared<TextureHandle>(TextureType::kColor)};
        ImageAttachment gbuffer2{.image = std::make_shared<TextureHandle>(TextureType::kColor)};
        ImageAttachment gbuffer3{.image = std::make_shared<TextureHandle>(TextureType::kColor)};

        ImageAttachment bright_pass{.image = std::make_shared<TextureHandle>(TextureType::kColor),
                                    .extent = {512, 512},
                                    .format = VK_FORMAT_R16G16B16_SFLOAT};
        ImageAttachment blur_y{.image = std::make_shared<TextureHandle>(TextureType::kColor),
                               .extent = {512, 512},
                               .format = VK_FORMAT_R16G16B16_SFLOAT};

        ImageAttachment lum_64{.image = std::make_shared<TextureHandle>(TextureType::kColor),
                               .extent = {64, 64},
                               .format = VK_FORMAT_R16_SFLOAT};
        ImageAttachment lum_32{.image = std::make_shared<TextureHandle>(TextureType::kColor),
                               .extent = {32, 32},
                               .format = VK_FORMAT_R16_SFLOAT};
        ImageAttachment lum_16{.image = std::make_shared<TextureHandle>(TextureType::kColor),
                               .extent = {16, 16},
                               .format = VK_FORMAT_R16_SFLOAT};
        ImageAttachment lum_8{.image = std::make_shared<TextureHandle>(TextureType::kColor),
                              .extent = {8, 8},
                              .format = VK_FORMAT_R16_SFLOAT};
        ImageAttachment lum_4{.image = std::make_shared<TextureHandle>(TextureType::kColor),
                              .extent = {4, 4},
                              .format = VK_FORMAT_R16_SFLOAT};
        ImageAttachment lum_2{.image = std::make_shared<TextureHandle>(TextureType::kColor),
                              .extent = {2, 2},
                              .format = VK_FORMAT_R16_SFLOAT};
        ImageAttachment lum_1{.image = std::make_shared<TextureHandle>(TextureType::kColor),
                              .extent = {1, 1},
                              .format = VK_FORMAT_R16_SFLOAT};

        ImageAttachment lum_A{.image = std::make_shared<TextureHandle>(TextureType::kColor),
                              .extent = {1, 1},
                              .format = VK_FORMAT_R16_SFLOAT,
                              .initial_value = {{1e7f, 0.f, 0.f, 1.f}}};
        ImageAttachment lum_B{.image = std::make_shared<TextureHandle>(TextureType::kColor),
                              .extent = {1, 1},
                              .format = VK_FORMAT_R16_SFLOAT,
                              .initial_value = {{1e7f, 0.f, 0.f, 1.f}}};

        BufferResource perFrameDataBuffer = {};
        BufferResource materialBuffer = {};
        BufferResource lightBuffer = {};
        BufferResource meshBuffer = {};
      } resources_;

      std::vector<ImageAttachment> attachment_list_;

    private:
      IGpu* gpu_ = nullptr;
      std::unordered_map<std::string, VkShaderModule> shader_cache_{};
    };

    class RenderPass : public NonCopyable<RenderPass>{
    public:
      RenderPass() = default;
      void init(IGpu* gpu,
                ResourceManager* resource_manager,
                ResourceRegistry* registry,
                Repository* repository, FrameProvider* frame) {
        gpu_ = gpu;
        resource_manager_ = resource_manager;
        registry_ = registry;
        repository_ = repository;
        frame_ = frame;

        post_process_sampler = repository_->get_sampler({
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .anisotropyEnable = VK_FALSE,
        });

        prepare();
      }
      RenderPassDependency& get_dependencies() { return dependencies_; }

      virtual ~RenderPass() = default;

      virtual void execute(VkCommandBuffer cmd) = 0;
      [[nodiscard]] virtual std::string get_name() const = 0;
      virtual void cleanup();

    protected:
      virtual void prepare() = 0;
      virtual void destroy() = 0;

      void begin_renderpass(VkCommandBuffer cmd);
      static void bind_descriptor_buffers(VkCommandBuffer cmd, const std::vector<DescriptorBuffer*>& descriptor_buffers);
      void begin_compute_pass(VkCommandBuffer cmd);

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
      VkSampler post_process_sampler = nullptr;

      FrameProvider* frame_;
      IGpu* gpu_;
      ResourceManager* resource_manager_;
      ResourceRegistry* registry_;
      Repository* repository_;
    };

    class VkSwapchain : public NonCopyable<VkSwapchain> {
      IGpu* gpu_;

    public:
      VkSwapchainKHR swapchain;
      VkFormat swapchain_image_format;
      VkExtent2D swapchain_extent;
      VkExtent2D draw_extent;

      std::vector<std::shared_ptr<TextureHandle>> swapchain_images;

      void init(IGpu* gpu, const VkExtent3D& extent);
      void create_swapchain(uint32_t width, uint32_t height);
      void resize_swapchain(Window* window);
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

    class RenderEngine : public NonCopyable<RenderEngine> {
      IGpu* gpu_;
      Window* window_;
      ResourceManager* resource_manager_;
      Repository* repository_;
      Gui* imgui_;
      FrameProvider* frame_;

      std::unique_ptr<ResourceRegistry> resource_registry_ = std::make_unique<ResourceRegistry>();

      std::unique_ptr<VkSwapchain> swapchain_ = std::make_unique<VkSwapchain>();

      std::vector<std::shared_ptr<RenderPass>> render_passes_;

      std::shared_ptr<TextureHandle> debug_texture_;

      bool resize_requested_{false};
      uint32 swapchain_image_index_{0};
      FrameData frames_{};
      bool acquire_next_image();
      void present(VkCommandBuffer cmd);

      auto& frame() { return frames_.get_current_frame(); }

      void create_resources() const;
      VkCommandBuffer start_draw();
      void execute(const std::shared_ptr<RenderPass>& render_pass, VkCommandBuffer cmd);

    public:
      void init(IGpu* gpu, Window* window, ResourceManager* resource_manager,
                Repository* repository, Gui* imgui_gui, FrameProvider* frame);
      void execute_passes();

      void cleanup();

      RenderConfig& get_config() const { return resource_registry_->config_; }
      VkFormat get_swapchain_format() const { return swapchain_->swapchain_image_format; }
      std::shared_ptr<TextureHandle> get_debug_image() const { return debug_texture_; }
    };

}  // namespace gestalt