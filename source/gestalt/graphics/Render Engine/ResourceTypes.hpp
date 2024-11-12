#pragma once

#include <memory>
#include <vector>

#include "common.hpp"
#include "VulkanTypes.hpp"

namespace gestalt::foundation {
  struct BufferCollection;
}

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
      kTask = VK_SHADER_STAGE_TASK_BIT_EXT,
      kMesh = VK_SHADER_STAGE_MESH_BIT_EXT
    };

    struct ShaderProgram {
      ShaderStage stage;
      std::string source_path;
    };

    struct ImageAttachment {
      std::string name;
      std::shared_ptr<TextureHandle> image;
      float32 scale{1.0f};
      VkExtent3D extent{0, 0, 0};
      VkFormat format{VK_FORMAT_UNDEFINED};
      VkClearValue initial_value{};
      uint32 version{0};
    };

    enum class ImageUsageType {
      kRead,               // will only be read as input to the shader
      kWrite,              // will only be written as attachment by the shader
      kStore,             // will be written by store operation
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


}  // namespace gestalt