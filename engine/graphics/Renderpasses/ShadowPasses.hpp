#pragma once
#include "Utils/CommandBuffer.hpp"
#include "Utils/Pipeline.hpp"
#include "Utils/ResourceBindings.hpp"

#include "RenderPass.hpp"

namespace gestalt::graphics {
	
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
        IGpu& gpu,
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
          compute_pipeline_(&gpu, get_name(), resources_.get_image_bindings(),
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
        IGpu& gpu)
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
          compute_pipeline_(&gpu, get_name(), resources_.get_image_bindings(),
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
                                const std::shared_ptr<ImageInstance>& shadow_map, IGpu& gpu)
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
              &gpu, get_name(), resources_.get_image_bindings(), resources_.get_buffer_bindings(),
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
}
