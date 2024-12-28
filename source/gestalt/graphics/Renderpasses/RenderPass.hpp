#pragma once
#include "Utils/CommandBuffer.hpp"
#include "Utils/Pipeline.hpp"
#include "Utils/ResourceBindings.hpp"
#include <RenderConfig.hpp>
#include <PerFrameData.hpp>

#include "RenderConfig.hpp"


namespace gestalt::graphics::fg {
	
  class RenderPass : Moveable<RenderPass> {
    std::string name_;
  protected:
    explicit RenderPass(std::string name) : name_(std::move(name)) {
      fmt::println("Compiling Render Pass: {}", name_);
    }

  public:
    [[nodiscard]] std::string_view get_name() const { return name_; }
    virtual std::vector<ResourceBinding<ResourceInstance>> get_resources(ResourceUsage usage) = 0;
    virtual std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers()
        = 0; 
    virtual void execute(CommandBuffer cmd) = 0;
    virtual ~RenderPass() = default;
  };

  class DrawCullDirectionalDepthPass : public RenderPass {
    // TODO
    struct alignas(16) DrawCullDepthConstants {
      int32 draw_count;
    };
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<int32()> draw_count_provider_;  // function to compute draw count

  public:
    DrawCullDirectionalDepthPass(const std::shared_ptr<BufferInstance>& camera_buffer,
                                 const std::shared_ptr<BufferInstance>& task_commands,
                                 const std::shared_ptr<BufferInstance>& draws,
                                 const std::shared_ptr<BufferInstance>& command_count,
        IGpu* gpu,
                                 std::function<int32()> draw_count_provider)
        : RenderPass("Draw Cull Directional Depth Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, camera_buffer, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 6, draws, ResourceUsage::READ, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 5, task_commands, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 7, command_count, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_push_constant(sizeof(DrawCullDepthConstants), VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_image_array_bindings(),
                            resources_.get_push_constant_range(),
                            "draw_cull_depth.comp.spv"),
          draw_count_provider_(std::move(draw_count_provider)) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers() override {
      return compute_pipeline_.get_descriptor_buffers();
    }

    void execute(const CommandBuffer cmd) override {
      const auto command_count = resources_.get_buffer_binding(1, 7).resource;

      cmd.global_barrier();

      cmd.fill_buffer(command_count->get_buffer_handle(), 0, command_count->get_size(), 0);

      cmd.global_barrier();

      const int32 max_command_count = draw_count_provider_();
      const uint32 group_count
          = (static_cast<uint32>(max_command_count) + 63) / 64;  // 64 threads per group

      const DrawCullDepthConstants draw_cull_constants{.draw_count = max_command_count};

      compute_pipeline_.bind(cmd);

      cmd.push_constants(compute_pipeline_.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(DrawCullDepthConstants), &draw_cull_constants);
      cmd.dispatch(group_count, 1, 1);
    }
  };

  class TaskSubmitDirectionalDepthPass final : public RenderPass {
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;

  public:
    TaskSubmitDirectionalDepthPass(const std::shared_ptr<BufferInstance>& task_commands,
                                   const std::shared_ptr<BufferInstance>& command_count, 
                                   const std::shared_ptr<BufferInstance>& group_count, 
        IGpu* gpu)
        : RenderPass("Task Submit Directional Depth Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 5,task_commands, ResourceUsage::READ, 
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(0, 7,command_count, ResourceUsage::READ, 
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(0, 8,group_count, ResourceUsage::WRITE, 
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
          )),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_image_array_bindings(),
                            resources_.get_push_constant_range(),
                            "task_submit.comp.spv") {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers() override {
      return compute_pipeline_.get_descriptor_buffers();
    }

    void execute(const CommandBuffer cmd) override {
      const auto group_count = resources_.get_buffer_binding(0, 8).resource;

      compute_pipeline_.bind(cmd);
      cmd.dispatch(1, 1, 1);
    }

  };

  class MeshletDirectionalDepthPass final : public RenderPass {
    struct alignas(16) MeshletDepthPushConstants {
      int cullFlags{0};
      float32 pyramidWidth, pyramidHeight;  // depth pyramid size in texels
      int32 clusterOcclusionEnabled;
    };

    ResourceComponent resources_;
    GraphicsPipeline graphics_pipeline_;

  public:
    MeshletDirectionalDepthPass(const std::shared_ptr<BufferInstance>& camera_buffer,
                                const std::shared_ptr<BufferInstance>& light_matrices,
                                const std::shared_ptr<BufferInstance>& directional_light,
                                const std::shared_ptr<BufferInstance>& point_light,
                                const std::shared_ptr<BufferInstance>& vertex_positions,
                                const std::shared_ptr<BufferInstance>& vertex_data,
                                const std::shared_ptr<BufferInstance>& meshlet,
                                const std::shared_ptr<BufferInstance>& meshlet_vertices,
                                const std::shared_ptr<BufferInstance>& meshlet_indices,
                                const std::shared_ptr<BufferInstance>& task_commands,
                                const std::shared_ptr<BufferInstance>& draws,
                                const std::shared_ptr<BufferInstance>& group_count,
                                const std::shared_ptr<ImageInstance>& shadow_map, IGpu* gpu)
        : RenderPass("Meshlet Directional Depth Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding( 0, 0,camera_buffer, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT
                                   | VK_SHADER_STAGE_FRAGMENT_BIT)
                  .add_binding( 1, 0,light_matrices, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_MESH_BIT_EXT)
                  .add_binding(1, 1,directional_light, ResourceUsage::READ, 
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_MESH_BIT_EXT)
                  .add_binding(1, 2,point_light, ResourceUsage::READ, 
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_MESH_BIT_EXT)
                  .add_binding( 2, 0,vertex_positions, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 1,vertex_data, ResourceUsage::READ, 
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 2,meshlet, ResourceUsage::READ, 
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 3,meshlet_vertices, ResourceUsage::READ, 
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding( 2, 4,meshlet_indices, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 5,task_commands, ResourceUsage::READ, 
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 6,draws, ResourceUsage::READ,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 8, group_count, ResourceUsage::READ,  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_push_constant(sizeof(MeshletDepthPushConstants),
                                     VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT)
                  .add_attachment(shadow_map))),

          graphics_pipeline_(
              gpu, get_name(), resources_.get_image_bindings(), resources_.get_buffer_bindings(),
              resources_.get_image_array_bindings(),
              resources_.get_push_constant_range(), "geometry_depth.task.spv",
              "geometry_depth.mesh.spv", "geometry_depth.frag.spv",
              GraphicsPipelineBuilder()
                  .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                  .set_polygon_mode(VK_POLYGON_MODE_FILL)
                  .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                  .set_multisampling_none()
                  .disable_blending()
                  .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
                  .set_depth_format(resources_.get_depth_attachment())
                  .build_pipeline_info()) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers() override {
      return graphics_pipeline_.get_descriptor_buffers();
    }

    void execute(const CommandBuffer cmd) override {
      auto clear_depth = VkClearDepthStencilValue{1.0f, 0};
      graphics_pipeline_.begin_render_pass(cmd, resources_.get_color_attachments(),
                                           resources_.get_depth_attachment(), std::nullopt,
                                           {clear_depth});
      graphics_pipeline_.bind(cmd);

      constexpr MeshletDepthPushConstants draw_cull_constants{.cullFlags = 1};
      cmd.push_constants(graphics_pipeline_.get_pipeline_layout(),
                         VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT, 0,
                         sizeof(MeshletDepthPushConstants), &draw_cull_constants);
      const auto group_count = resources_.get_buffer_binding(2, 8).resource;
      cmd.draw_mesh_tasks_indirect_ext(group_count->get_buffer_handle(), 0, 1, 0);
      cmd.end_rendering();
    }
  };

  
  class DrawCullPass final : public RenderPass {
    struct alignas(16) DrawCullConstants {
      int32 draw_count;
    };
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<int32()> draw_count_provider_;  // Function to compute draw count

  public:
    DrawCullPass(const std::shared_ptr<BufferInstance>& camera_buffer,
                                 const std::shared_ptr<BufferInstance>& task_commands,
                                 const std::shared_ptr<BufferInstance>& draws,
                                 const std::shared_ptr<BufferInstance>& command_count, IGpu* gpu,
                                 std::function<int32()> draw_count_provider)
        : RenderPass("Draw Cull Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, camera_buffer, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 6, draws, ResourceUsage::READ, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 5, task_commands, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 7, command_count, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_push_constant(sizeof(DrawCullConstants), VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_image_array_bindings(),
                            resources_.get_push_constant_range(),
                            "draw_cull.comp.spv"),
          draw_count_provider_(std::move(draw_count_provider)) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers() override {
      return compute_pipeline_.get_descriptor_buffers();
    }

    void execute(const CommandBuffer cmd) override {
      const auto command_count = resources_.get_buffer_binding(1, 7).resource;

      cmd.global_barrier();
      cmd.fill_buffer(command_count->get_buffer_handle(), 0, command_count->get_size(), 0);
      cmd.global_barrier();

      const int32 max_command_count = draw_count_provider_();
      const uint32 group_count
          = (static_cast<uint32>(max_command_count) + 63) / 64;  // 64 threads per group

      const DrawCullConstants draw_cull_constants{.draw_count = max_command_count};

      compute_pipeline_.bind(cmd);

      cmd.push_constants(compute_pipeline_.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(DrawCullConstants), &draw_cull_constants);
      cmd.dispatch(group_count, 1, 1);
    }
  };

  class TaskSubmitPass final : public RenderPass {
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;

  public:
    TaskSubmitPass(const std::shared_ptr<BufferInstance>& task_commands,
                                   const std::shared_ptr<BufferInstance>& command_count,
                                   const std::shared_ptr<BufferInstance>& group_count,
        IGpu* gpu)
        : RenderPass("Task Submit Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 5, task_commands, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(0, 7, command_count, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(0, 8, group_count, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
          )),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_image_array_bindings(),
                            resources_.get_push_constant_range(),
                            "task_submit.comp.spv") {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    
    std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers() override {
      return compute_pipeline_.get_descriptor_buffers();
    }

    void execute(const CommandBuffer cmd) override {
      const auto group_count = resources_.get_buffer_binding(0, 8).resource;
      
      compute_pipeline_.bind(cmd);
      cmd.dispatch(1, 1, 1);
    }
  };

  class MeshletPass final : public RenderPass {
    ResourceComponent resources_;
    GraphicsPipeline graphics_pipeline_;

    struct alignas(16) MeshletPushConstants {
      int cullFlags{0};
      float32 pyramidWidth, pyramidHeight;  // depth pyramid size in texels
      int32 clusterOcclusionEnabled;
    };

  public:
    MeshletPass(const std::shared_ptr<BufferInstance>& camera_buffer,
                const std::shared_ptr<BufferInstance>& materials,
                const std::shared_ptr<ImageArrayInstance>& textures,
                const std::shared_ptr<BufferInstance>& vertex_positions,
                const std::shared_ptr<BufferInstance>& vertex_data,
                const std::shared_ptr<BufferInstance>& meshlet,
                const std::shared_ptr<BufferInstance>& meshlet_vertices,
                const std::shared_ptr<BufferInstance>& meshlet_indices,
                const std::shared_ptr<BufferInstance>& task_commands,
                const std::shared_ptr<BufferInstance>& draws,
                const std::shared_ptr<BufferInstance>& group_count,
                const std::shared_ptr<ImageInstance>& g_buffer_1,
                const std::shared_ptr<ImageInstance>& g_buffer_2,
                const std::shared_ptr<ImageInstance>& g_buffer_3,
                const std::shared_ptr<ImageInstance>& g_buffer_depth, IGpu* gpu)
        : RenderPass("Meshlet Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, camera_buffer, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT
                                   | VK_SHADER_STAGE_FRAGMENT_BIT)
              .add_binding(1, 3, textures, ResourceUsage::READ,
                           VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                  .add_binding(1, 4, materials, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                  .add_binding(2, 0, vertex_positions, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 1, vertex_data, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 2, meshlet, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 3, meshlet_vertices, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 4, meshlet_indices, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 5, task_commands, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 6, draws, ResourceUsage::READ, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                               VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_binding(2, 8, group_count, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                VK_SHADER_STAGE_TASK_BIT_EXT)
                  .add_push_constant(sizeof(MeshletPushConstants),
                                     VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT)
                  .add_attachment(g_buffer_1, 0)
                  .add_attachment(g_buffer_2, 1)
                  .add_attachment(g_buffer_3, 2)
                  .add_attachment(g_buffer_depth))),
          graphics_pipeline_(
              gpu, get_name(), resources_.get_image_bindings(), resources_.get_buffer_bindings(),
              resources_.get_image_array_bindings(),
              resources_.get_push_constant_range(), "geometry.task.spv", "geometry.mesh.spv",
              "geometry_deferred.frag.spv",
              GraphicsPipelineBuilder()
                  .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                  .set_polygon_mode(VK_POLYGON_MODE_FILL)
                  .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                  .set_multisampling_none()
                  .disable_blending(3)
                  .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
                  .set_color_attachment_formats(resources_.get_color_attachments())
                  .set_depth_format(resources_.get_depth_attachment())
                  .build_pipeline_info()) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers() override {
      return graphics_pipeline_.get_descriptor_buffers();
    }

    void execute(const CommandBuffer cmd) override {
      auto clear_color = VkClearColorValue{0.0f, 0.0f, 0.0f, 0.0f};
      auto clear_depth = VkClearDepthStencilValue{1.0f, 0};
      graphics_pipeline_.begin_render_pass(cmd, resources_.get_color_attachments(),
                                           resources_.get_depth_attachment(), {clear_color},
                                           {clear_depth});
      graphics_pipeline_.bind(cmd);

      constexpr MeshletPushConstants draw_cull_constants{};
      cmd.push_constants(graphics_pipeline_.get_pipeline_layout(),
                         VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT, 0,
                         sizeof(MeshletPushConstants), &draw_cull_constants);
      const auto group_count = resources_.get_buffer_binding(2, 8).resource;
      cmd.draw_mesh_tasks_indirect_ext(group_count->get_buffer_handle(), 0, 1, 0);
      cmd.end_rendering();
    }
  };

  class SsaoPass final : public RenderPass {
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<RenderConfig::SsaoParams()>
        push_constant_provider_; 
  public:
    SsaoPass(const std::shared_ptr<BufferInstance>& camera_buffer,
             const std::shared_ptr<ImageInstance>& g_buffer_depth,
             const std::shared_ptr<ImageInstance>& g_buffer_2,
             const std::shared_ptr<ImageInstance>& rotation_texture,
             const std::shared_ptr<ImageInstance>& ambient_occlusion,
             const VkSampler post_process_sampler, IGpu* gpu,
             const std::function<RenderConfig::SsaoParams()>& push_constant_provider)
        : RenderPass("Ssao Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, camera_buffer, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 0, g_buffer_depth, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 1, g_buffer_2, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 2, rotation_texture, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 3, ambient_occlusion, nullptr, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_push_constant(sizeof(RenderConfig::SsaoParams),
                                     VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_image_array_bindings(),
                            resources_.get_push_constant_range(),
                            "ssao_filter.comp.spv"),
          push_constant_provider_(push_constant_provider) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers() override {
      return compute_pipeline_.get_descriptor_buffers();
    }

    void execute(const CommandBuffer cmd) override {
      compute_pipeline_.bind(cmd);

      const auto push_constants = push_constant_provider_();
      cmd.push_constants(compute_pipeline_.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(RenderConfig::SsaoParams), &push_constants);
      const auto [width, height, _] = resources_.get_image_binding(1, 3).resource->get_extent();
      cmd.dispatch(static_cast<uint32>(ceil(width / 8)), static_cast<uint32>(ceil(height / 8)), 1);
    }
  };

  class VolumetricLightingInjectionPass final : public RenderPass {
    struct alignas(16) VolumetricLightingInjectionPassConstants {
      glm::vec2 halton_xy{0.f, 0.f};  // no jitter
      float32 temporal_reprojection_jitter_scale{0.f};
      float32 density_modifier{1.f};

      glm::uvec3 froxel_dimensions{128, 128, 128};
      int32 current_frame{0};

      float32 noise_scale{1.f};
      uint32 noise_type{0};
      float32 froxel_near{1.f};
      float32 froxel_far{100.f};

      glm::mat4 froxel_inverse_view_projection{1.f};

      float32 volumetric_noise_position_multiplier{1.f};
      float32 volumetric_noise_speed_multiplier{1.f};
      float32 height_fog_density{0.01f};
      float32 height_fog_falloff{1.f};

      glm::vec3 box_position{0.f};
      float32 box_fog_density{0.1f};

      glm::vec3 box_size{10.f};
      float32 scattering_factor{0.5f};
    };
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<RenderConfig::VolumetricLightingParams()> push_constant_provider_;
    std::function<uint32()> frame_provider_;
    std::function<PerFrameData()> camera_provider_;

  public:
    VolumetricLightingInjectionPass(
        const std::shared_ptr<ImageInstance>& blue_noise,
        const std::shared_ptr<ImageInstance>& noise_texture,
        const std::shared_ptr<ImageInstance>& froxel_data, const VkSampler post_process_sampler,
        const std::function<RenderConfig::VolumetricLightingParams()>& push_constant_provider,
        const std::function<uint32()>& frame_provider,
        const std::function<PerFrameData()>& camera_provider, IGpu* gpu)
        : RenderPass("Volumetric Lighting Injection Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, blue_noise, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(0, 1, noise_texture, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(0, 2, froxel_data, nullptr, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_push_constant(sizeof(VolumetricLightingInjectionPassConstants),
                                     VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_image_array_bindings(),
                            resources_.get_push_constant_range(),
                            "volumetric_light_injection.comp.spv"),
          push_constant_provider_(push_constant_provider),
          frame_provider_(frame_provider),
          camera_provider_(camera_provider) {}

   std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

   std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers() override {
     return compute_pipeline_.get_descriptor_buffers();
   }

    void execute(const CommandBuffer cmd) override {
      compute_pipeline_.bind(cmd);

      const auto params = push_constant_provider_();
      const auto perFrameData = camera_provider_();
      VolumetricLightingInjectionPassConstants push_constants;
      push_constants.current_frame = frame_provider_();
      push_constants.froxel_inverse_view_projection = perFrameData.inv_viewProj;
      push_constants.density_modifier = params.density_modifier;
      push_constants.noise_scale = params.noise_scale;
      push_constants.noise_type = params.noise_type;
      push_constants.volumetric_noise_position_multiplier
          = params.volumetric_noise_position_multiplier;
      push_constants.froxel_near = perFrameData.znear;
      push_constants.froxel_far = perFrameData.zfar;
      push_constants.height_fog_density = params.height_fog_density;
      push_constants.height_fog_falloff = params.height_fog_falloff;
      push_constants.box_position = params.box_position;
      push_constants.box_fog_density = params.box_fog_density;
      push_constants.box_size = params.box_size;
      push_constants.scattering_factor = params.scattering_factor;

      cmd.push_constants(compute_pipeline_.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(VolumetricLightingInjectionPassConstants), &push_constants);
      const auto [width, height, _] = resources_.get_image_binding(0, 2).resource->get_extent();
      cmd.dispatch(static_cast<uint32>(ceil(width / 8)), static_cast<uint32>(ceil(height / 8)), 128);
    }
  };

  class VolumetricLightingScatteringPass final : public RenderPass {
    struct alignas(16) VolumetricLightingScatteringPassConstants {
      glm::vec2 halton_xy{0.f, 0.f};  // no jitter
      float32 temporal_reprojection_jitter_scale{0.f};
      float32 density_modifier{1.f};

      glm::uvec3 froxel_dimensions{128, 128, 128};
      int32 current_frame{0};

      float32 noise_scale{1.f};
      uint32 noise_type{0};
      float32 froxel_near{1.f};
      float32 froxel_far{100.f};

      glm::mat4 froxel_inverse_view_projection{1.f};

      float32 scattering_factor{0.5f};
      float32 phase_anisotropy{0.2f};
      uint32 phase_type{0};
      uint32 num_point_lights;
    };
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<RenderConfig::VolumetricLightingParams()> push_constant_provider_;
    std::function<uint32()> frame_provider_;
    std::function<PerFrameData()> camera_provider_;
    std::function<uint32()> point_light_count_provider_;

  public:
    VolumetricLightingScatteringPass(
        const std::shared_ptr<BufferInstance>& camera_buffer,
        const std::shared_ptr<BufferInstance>& light_matrices,
        const std::shared_ptr<BufferInstance>& directional_lights,
        const std::shared_ptr<BufferInstance>& points_lights,
        const std::shared_ptr<ImageInstance>& blue_noise,
        const std::shared_ptr<ImageInstance>& froxel_data,
        const std::shared_ptr<ImageInstance>& shadow_map,
        const std::shared_ptr<ImageInstance>& light_scattering,
        const VkSampler post_process_sampler,
        const std::function<RenderConfig::VolumetricLightingParams()>& push_constant_provider,
        const std::function<uint32()>& frame_provider,
        const std::function<PerFrameData()>& camera_provider,
        const std::function<uint32()>& point_light_count_provider, IGpu* gpu)
        : RenderPass("Volumetric Lighting Scattering Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, camera_buffer, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)

                  .add_binding(1, 0, blue_noise, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 1, froxel_data, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 2, shadow_map, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 3, light_scattering, nullptr, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                               VK_SHADER_STAGE_COMPUTE_BIT)

                  .add_binding(2, 0, light_matrices, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(2, 1, directional_lights, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(2, 2, points_lights, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_push_constant(sizeof(VolumetricLightingScatteringPassConstants),
                                     VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_image_array_bindings(),
                            resources_.get_push_constant_range(),
                            "volumetric_light_scattering.comp.spv"),
          push_constant_provider_(push_constant_provider),
          frame_provider_(frame_provider),
          camera_provider_(camera_provider),
          point_light_count_provider_(point_light_count_provider) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers() override {
      return compute_pipeline_.get_descriptor_buffers();
    }

    void execute(const CommandBuffer cmd) override {
      compute_pipeline_.bind(cmd);

      const auto params = push_constant_provider_();
      const auto perFrameData = camera_provider_();
      VolumetricLightingScatteringPassConstants push_constants;
      push_constants.current_frame = frame_provider_();
      push_constants.froxel_inverse_view_projection = perFrameData.inv_viewProj;
      push_constants.froxel_near = perFrameData.znear;
      push_constants.froxel_far = perFrameData.zfar;
      push_constants.scattering_factor = params.scattering_factor;
      push_constants.phase_anisotropy = params.phase_anisotropy;
      push_constants.phase_type = params.phase_type;
      push_constants.density_modifier = params.density_modifier;
      push_constants.noise_scale = params.noise_scale;
      push_constants.noise_type = params.noise_type;
      push_constants.num_point_lights = point_light_count_provider_();


      cmd.push_constants(compute_pipeline_.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(VolumetricLightingScatteringPassConstants), &push_constants);
      const auto [width, height, _] = resources_.get_image_binding(1, 3).resource->get_extent();
      cmd.dispatch(static_cast<uint32>(ceil(width / 8)), static_cast<uint32>(ceil(height / 8)), 128);
    }
  };

  
  class VolumetricLightingSpatialFilterPass final : public RenderPass {
    struct VolumetricLightingSpatialFilterPassConstants {
      glm::uvec3 froxel_dimensions{128, 128, 128};
      int32 use_spatial_filtering{1};
    };
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<RenderConfig::VolumetricLightingParams()> push_constant_provider_;

  public:
    VolumetricLightingSpatialFilterPass(
        const std::shared_ptr<ImageInstance>& light_scattering,
        const std::shared_ptr<ImageInstance>& froxel_data, const VkSampler post_process_sampler,
        const std::function<RenderConfig::VolumetricLightingParams()>& push_constant_provider,
        IGpu* gpu)
        : RenderPass("Volumetric Lighting Spatial Filter Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, light_scattering, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(0, 1, froxel_data, nullptr, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_push_constant(sizeof(VolumetricLightingSpatialFilterPassConstants),
                                     VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_image_array_bindings(),
                            resources_.get_push_constant_range(),
                            "volumetric_light_spatial_filter.comp.spv"),
          push_constant_provider_(push_constant_provider) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers() override {
      return compute_pipeline_.get_descriptor_buffers();
    }

    void execute(const CommandBuffer cmd) override {
      compute_pipeline_.bind(cmd);

      const auto params = push_constant_provider_();
      VolumetricLightingSpatialFilterPassConstants push_constants;
      push_constants.use_spatial_filtering = params.enable_spatial_filter;

      cmd.push_constants(compute_pipeline_.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(VolumetricLightingSpatialFilterPassConstants), &push_constants);
      const auto [width, height, _] = resources_.get_image_binding(0, 1).resource->get_extent();
      cmd.dispatch(static_cast<uint32>(ceil(width / 8)), static_cast<uint32>(ceil(height / 8)), 128);
    }
  };

  class VolumetricLightingIntegrationPass final : public RenderPass {
    struct alignas(16) VolumetricLightingIntegrationPassConstants {
      glm::uvec3 froxel_dimensions{128, 128, 128};
      int32 current_frame{0};

      float32 noise_scale{1.f};
      uint32 noise_type{0};
      float32 froxel_near{1.f};
      float32 froxel_far{100.f};
    };
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<RenderConfig::VolumetricLightingParams()> push_constant_provider_;
    std::function<uint32()> frame_provider_;
    std::function<PerFrameData()> camera_provider_;

  public:
    VolumetricLightingIntegrationPass(
        const std::shared_ptr<ImageInstance>& froxel_data,
        const std::shared_ptr<ImageInstance>& light_integrated,
        const VkSampler post_process_sampler,
        const std::function<RenderConfig::VolumetricLightingParams()>& push_constant_provider,
        const std::function<uint32()>& frame_provider,
        const std::function<PerFrameData()>& camera_provider, IGpu* gpu)
        : RenderPass("Volumetric Lighting Integration Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, froxel_data, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(0, 1, light_integrated, nullptr, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_push_constant(sizeof(VolumetricLightingIntegrationPassConstants),
                                     VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_image_array_bindings(),
                            resources_.get_push_constant_range(),
                            "volumetric_light_spatial_filter.comp.spv"),
          push_constant_provider_(push_constant_provider),
          frame_provider_(frame_provider),
          camera_provider_(camera_provider)
    {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers() override {
      return compute_pipeline_.get_descriptor_buffers();
    }

    void execute(const CommandBuffer cmd) override {
      compute_pipeline_.bind(cmd);

      const auto params = push_constant_provider_();
      const auto perFrameData = camera_provider_();
      VolumetricLightingIntegrationPassConstants push_constants;
      push_constants.current_frame = frame_provider_();
      push_constants.froxel_near = perFrameData.znear;
      push_constants.froxel_far = perFrameData.zfar;
      push_constants.noise_scale = params.noise_scale;
      push_constants.noise_type = params.noise_type;


      cmd.push_constants(compute_pipeline_.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(VolumetricLightingIntegrationPassConstants), &push_constants);
      const auto [width, height, _] = resources_.get_image_binding(0, 1).resource->get_extent();
      cmd.dispatch(static_cast<uint32>(ceil(width / 8)), static_cast<uint32>(ceil(height / 8)), 128);
    }
  };

  
  class VolumetricLightingNoisePass final : public RenderPass {
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<RenderConfig::VolumetricLightingParams()> push_constant_provider_;
    std::function<uint32()> frame_provider_;
    std::function<PerFrameData()> camera_provider_;

  public:
    VolumetricLightingNoisePass(const std::shared_ptr<ImageInstance>& volumetric_noise_texture,
                                IGpu* gpu)
        : RenderPass("Volumetric Lighting Noise Pass"),
          resources_(std::move(
              ResourceComponentBindings().add_binding(
              0, 0, volumetric_noise_texture, nullptr, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
)),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_image_array_bindings(),
                            resources_.get_push_constant_range(),
                            "volumetric_light_noise.comp.spv") {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers() override {
      return compute_pipeline_.get_descriptor_buffers();
    }

    void execute(const CommandBuffer cmd) override {
      compute_pipeline_.bind(cmd);
const auto [width, height, _] = resources_.get_image_binding(0, 0).resource->get_extent();
      cmd.dispatch(static_cast<uint32>(ceil(width / 8)), static_cast<uint32>(ceil(height / 8)), 128);
    }
  };

  class LightingPass final : public RenderPass {
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    const std::function<RenderConfig::LightingParams()> push_constant_provider_;
    std::function<PerFrameData()> camera_provider_;
    std::function<uint32()> dir_light_count_provider_;
    std::function<uint32()> point_light_count_provider_;

  public:
    LightingPass(const std::shared_ptr<BufferInstance>& camera_buffer,
                 const std::shared_ptr<ImageInstance>& tex_env_map,
                 const std::shared_ptr<ImageInstance>& tex_env_map_irradiance,
                 const std::shared_ptr<ImageInstance>& tex_bdrf_lut,
                 const std::shared_ptr<BufferInstance>& light_matrices,
                 const std::shared_ptr<BufferInstance>& directional_light,
                 const std::shared_ptr<BufferInstance>& point_light,
                 const std::shared_ptr<ImageInstance>& g_buffer_1,
                 const std::shared_ptr<ImageInstance>& g_buffer_2,
                 const std::shared_ptr<ImageInstance>& g_buffer_3,
                 const std::shared_ptr<ImageInstance>& g_buffer_depth,
                 const std::shared_ptr<ImageInstance>& shadow_map,
                 const std::shared_ptr<ImageInstance>& integrated_light_scattering,
                 const std::shared_ptr<ImageInstance>& ambient_occlusion,
                 const std::shared_ptr<ImageInstance>& scene_lit,
                 const VkSampler post_process_sampler, IGpu* gpu,
                 const std::function<RenderConfig::LightingParams()>& push_constant_provider,
                 const std::function<PerFrameData()>& camera_provider,
                 const std::function<uint32()>& dir_light_count_provider,
                 const std::function<uint32()>& point_light_count_provider)
        : RenderPass("Lighting Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, camera_buffer, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)

                  .add_binding(1, 0, tex_env_map, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 1, tex_env_map_irradiance, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(1, 2, tex_bdrf_lut, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)

                  .add_binding(2, 0, light_matrices, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(2, 1, directional_light, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(2, 2, point_light, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)

                  .add_binding(3, 0, g_buffer_1, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(3, 1, g_buffer_2, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(3, 2, g_buffer_3, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(3, 3, g_buffer_depth, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(3, 4, shadow_map, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(3, 5, integrated_light_scattering, post_process_sampler,
                               ResourceUsage::READ, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(3, 6, ambient_occlusion, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(3, 7, scene_lit, nullptr, ResourceUsage::WRITE,
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_push_constant(sizeof(RenderConfig::LightingParams),
                                     VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_image_array_bindings(),
                            resources_.get_push_constant_range(), "pbr_lighting.comp.spv"),
          push_constant_provider_(push_constant_provider),
          camera_provider_(camera_provider),
          dir_light_count_provider_(dir_light_count_provider),
          point_light_count_provider_(point_light_count_provider) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers() override {
      return compute_pipeline_.get_descriptor_buffers();
    }

    void execute(CommandBuffer cmd) override {
      compute_pipeline_.bind(cmd);

              auto params = push_constant_provider_();
      params.invViewProj = camera_provider_().inv_viewProj;
              params.num_dir_lights = dir_light_count_provider_();
      params.num_point_lights = point_light_count_provider_();
              cmd.push_constants(compute_pipeline_.get_pipeline_layout(),
                                 VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                 sizeof(RenderConfig::LightingParams), &params);

      const auto [width, height, _] = resources_.get_image_binding(3, 7).resource->get_extent();
      cmd.dispatch(static_cast<uint32>(ceil(width / 8)), static_cast<uint32>(ceil(height / 8)), 1);
    }
  };

  
  class SkyboxPass final : public RenderPass {
    ResourceComponent resources_;
    GraphicsPipeline graphics_pipeline_;
    std::function<RenderConfig::SkyboxParams()> push_constant_provider_;

  public:
    SkyboxPass(const std::shared_ptr<BufferInstance>& camera_buffer,
               const std::shared_ptr<BufferInstance>& directional_light,
               const std::shared_ptr<ImageInstance>& scene_lit,
               const std::shared_ptr<ImageInstance>& texEnvMap,
               const std::shared_ptr<ImageInstance>& scene_skybox,
                const std::shared_ptr<ImageInstance>& g_buffer_depth, 
        const VkSampler post_process_sampler,
        IGpu* gpu, const std::function<RenderConfig::SkyboxParams()>& push_constant_provider
    )
        : RenderPass("Skybox Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding(0, 0, camera_buffer, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                               VK_SHADER_STAGE_VERTEX_BIT
                                   | VK_SHADER_STAGE_FRAGMENT_BIT)
                  .add_binding(1, 1, directional_light, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                  .add_binding(2, 0, scene_lit, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                  .add_binding(2, 1, texEnvMap, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                  .add_push_constant(sizeof(RenderConfig::SkyboxParams),
                                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                  .add_attachment(scene_skybox)
                  .add_attachment(g_buffer_depth))),
          graphics_pipeline_(
              gpu, get_name(), resources_.get_image_bindings(), resources_.get_buffer_bindings(),
              resources_.get_image_array_bindings(), resources_.get_push_constant_range(),
              "skybox.vert.spv", "skybox.frag.spv",
              GraphicsPipelineBuilder()
                  .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                  .set_polygon_mode(VK_POLYGON_MODE_FILL)
                  .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                  .set_multisampling_none()
                  .disable_blending()
                  .enable_depthtest(false, VK_COMPARE_OP_LESS_OR_EQUAL)
                  .set_color_attachment_formats(resources_.get_color_attachments())
                  .set_depth_format(resources_.get_depth_attachment())
                  .build_pipeline_info()), push_constant_provider_(push_constant_provider) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(
        const ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers() override {
      return graphics_pipeline_.get_descriptor_buffers();
    }

    void execute(const CommandBuffer cmd) override {
      auto clear_color = VkClearColorValue{0.0f, 0.0f, 0.0f, 0.0f};
      graphics_pipeline_.begin_render_pass(cmd, resources_.get_color_attachments(),
                                           resources_.get_depth_attachment(), {clear_color});
      graphics_pipeline_.bind(cmd);

      auto push_constant = push_constant_provider_();
      cmd.push_constants(graphics_pipeline_.get_pipeline_layout(),
                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(RenderConfig::SkyboxParams), &push_constant);

      cmd.draw(36, 1, 0, 0);
      cmd.end_rendering();
    }
  };

  class ToneMapPass final : public RenderPass {
    ResourceComponent resources_;
    ComputePipeline compute_pipeline_;
    std::function<RenderConfig::HdrParams()> push_constant_provider_;

  public:
    ToneMapPass(const std::shared_ptr<ImageInstance>& scene_final,
                const std::shared_ptr<ImageInstance>& scene_lit,
                const std::shared_ptr<ImageInstance>& scene_skybox,
                const VkSampler post_process_sampler, IGpu* gpu,
                const std::function<RenderConfig::HdrParams()>& push_constant_provider
        )
        : RenderPass("Tone Map Pass"),
          resources_(std::move(
              ResourceComponentBindings()
                  .add_binding( 0, 0,scene_lit, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding( 0, 1,scene_skybox, post_process_sampler, ResourceUsage::READ,
                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_binding(0, 3, scene_final, nullptr, ResourceUsage::WRITE, 
                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                  .add_push_constant(sizeof(RenderConfig::HdrParams), VK_SHADER_STAGE_COMPUTE_BIT))),
          compute_pipeline_(gpu, get_name(), resources_.get_image_bindings(),
                            resources_.get_buffer_bindings(), resources_.get_image_array_bindings(),
                            resources_.get_push_constant_range(),
                            "tone_mapping.comp.spv"), push_constant_provider_(push_constant_provider) {}

    std::vector<ResourceBinding<ResourceInstance>> get_resources(ResourceUsage usage) override {
      return resources_.get_resources(usage);
    }

    std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers() override {
      return compute_pipeline_.get_descriptor_buffers();
    }

    void execute(CommandBuffer cmd) override {
      compute_pipeline_.bind(cmd);

      const auto params = push_constant_provider_();
      cmd.push_constants(compute_pipeline_.get_pipeline_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(RenderConfig::HdrParams), &params);

      const auto [width, height, _] = resources_.get_image_binding(0, 1).resource->get_extent();
      cmd.dispatch(static_cast<uint32>(ceil(width / 8)), static_cast<uint32>(ceil(height / 8)), 1);
    }
  };
}
