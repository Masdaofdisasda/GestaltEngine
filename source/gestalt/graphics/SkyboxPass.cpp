#include "RenderPass.hpp"

#include "vk_images.hpp"
#include "vk_initializers.hpp"
#include "vk_pipelines.hpp"
#include <fmt/core.h>

#include "FrameProvider.hpp"
#include "Interface/IGpu.hpp"

namespace gestalt::graphics {

  void SkyboxPass::prepare() {
    fmt::print("Preparing {}\n", get_name());

    dependencies_
        = RenderPassDependencyBuilder()
              .add_shader(ShaderStage::kVertex, "skybox.vert.spv")
              .add_shader(ShaderStage::kFragment, "skybox.frag.spv")
              .add_image_attachment(registry_->resources_.scene_color, ImageUsageType::kWrite, 1)
              .add_image_attachment(registry_->resources_.scene_depth, ImageUsageType::kWrite, 1)
              .set_push_constant_range(sizeof(RenderConfig::SkyboxParams),
                                       VK_SHADER_STAGE_FRAGMENT_BIT)
              .build();

    descriptor_layouts_.emplace_back(
        DescriptorLayoutBuilder()
            .add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT
                             | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_COMPUTE_BIT
                             | VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(gpu_->getDevice()));
    descriptor_layouts_.emplace_back(
        DescriptorLayoutBuilder()
            .add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                         VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, false,
                         getMaxLights())
            .add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, false,
                         getMaxDirectionalLights())
            .add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, false,
                         getMaxPointLights())
            .build(gpu_->getDevice()));

    create_pipeline_layout();

    pipeline_ = create_pipeline()
                    .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                    .set_polygon_mode(VK_POLYGON_MODE_FILL)
                    .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                    .set_multisampling_none()
                    .disable_blending()
                    .enable_depthtest(false, VK_COMPARE_OP_LESS_OR_EQUAL)
                    .build_graphics_pipeline(gpu_->getDevice());
  }

  void SkyboxPass::execute(const VkCommandBuffer cmd) {
    begin_renderpass(cmd);
    const auto frame = frame_->get_current_frame_index();

    const auto& per_frame_buffers = repository_->per_frame_data_buffers;
    const auto& light_data = repository_->light_buffers;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    bind_descriptor_buffers(
        cmd, {per_frame_buffers->descriptor_buffers[frame].get(), light_data->descriptor_buffer.get()
    });
    per_frame_buffers->descriptor_buffers[frame]->bind_descriptors(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout_, 0);
    light_data->descriptor_buffer->bind_descriptors(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout_, 1);
    vkCmdSetViewport(cmd, 0, 1, &viewport_);
    vkCmdSetScissor(cmd, 0, 1, &scissor_);

    vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(RenderConfig::SkyboxParams), &registry_->config_.skybox);
    vkCmdDraw(cmd, 36, 1, 0, 0);  // 36 vertices for the cube

    vkCmdEndRendering(cmd);
  }

  void SkyboxPass::destroy() {}

  void InfiniteGridPass::prepare() {
    fmt::print("Preparing {}\n", get_name());

    dependencies_
        = RenderPassDependencyBuilder()
              .add_shader(ShaderStage::kVertex, "infinite_grid.vert.spv")
              .add_shader(ShaderStage::kFragment, "infinite_grid.frag.spv")
              .add_image_attachment(registry_->resources_.scene_color, ImageUsageType::kWrite, 1)
              .add_image_attachment(registry_->resources_.scene_depth, ImageUsageType::kWrite, 1)
              .set_push_constant_range(sizeof(RenderConfig::GridParams),
                                       VK_SHADER_STAGE_FRAGMENT_BIT)
              .build();

    descriptor_layouts_.push_back(
        DescriptorLayoutBuilder()
            .add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT
                             | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_COMPUTE_BIT
                             | VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(gpu_->getDevice()));

    create_pipeline_layout();

    pipeline_ = create_pipeline()
                    .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                    .set_polygon_mode(VK_POLYGON_MODE_FILL)
                    .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                    .set_multisampling_none()
                    .disable_blending()
                    .enable_depthtest(false, VK_COMPARE_OP_LESS_OR_EQUAL)
                    .build_graphics_pipeline(gpu_->getDevice());
  }

  void InfiniteGridPass::execute(const VkCommandBuffer cmd) {
    begin_renderpass(cmd);
    const auto frame = frame_->get_current_frame_index();

    const auto& per_frame_buffers = repository_->per_frame_data_buffers;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    bind_descriptor_buffers(cmd, {per_frame_buffers->descriptor_buffers[frame].get()});
    per_frame_buffers->descriptor_buffers[frame]->bind_descriptors(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(RenderConfig::GridParams), &registry_->config_.grid);
    vkCmdSetViewport(cmd, 0, 1, &viewport_);
    vkCmdSetScissor(cmd, 0, 1, &scissor_);
    vkCmdDraw(cmd, 6, 1, 0, 0);  // 6 vertices for the grid

    vkCmdEndRendering(cmd);
  }

  void InfiniteGridPass::destroy() {}

  struct alignas(16) AabbBvhPushConstants {
    glm::vec4 max;
    glm::vec4 min;
  };

  void AabbBvhDebugPass::prepare() {
    fmt::print("Preparing {}\n", get_name());

    dependencies_
        = RenderPassDependencyBuilder()
              .add_shader(ShaderStage::kVertex, "debug_aabb.vert.spv")
              .add_shader(ShaderStage::kFragment, "debug_aabb.frag.spv")
              .add_image_attachment(registry_->resources_.scene_color, ImageUsageType::kWrite, 1)
              .add_image_attachment(registry_->resources_.scene_depth,
                                    ImageUsageType::kDepthStencilRead, 1)
              .set_push_constant_range(sizeof(AabbBvhPushConstants), VK_SHADER_STAGE_VERTEX_BIT)
              .build();

    descriptor_layouts_.push_back(
        DescriptorLayoutBuilder()
            .add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT
                             | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_COMPUTE_BIT
                             | VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(gpu_->getDevice()));

    create_pipeline_layout();

    pipeline_ = create_pipeline()
                    .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                    .set_polygon_mode(VK_POLYGON_MODE_LINE)
                    .set_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                    .set_multisampling_none()
                    .disable_blending()
                    .enable_depthtest(false, VK_COMPARE_OP_LESS_OR_EQUAL)
                    .build_graphics_pipeline(gpu_->getDevice());
  }

  void AabbBvhDebugPass::execute(const VkCommandBuffer cmd) {
    if (!registry_->config_.debug_aabb_bvh) {
      return;
    }

    begin_renderpass(cmd);
    const auto frame = frame_->get_current_frame_index();

    const auto& per_frame_buffers = repository_->per_frame_data_buffers;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    bind_descriptor_buffers(cmd, {per_frame_buffers->descriptor_buffers[frame].get()});
    per_frame_buffers->descriptor_buffers[frame]->bind_descriptors(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdSetViewport(cmd, 0, 1, &viewport_);
    vkCmdSetScissor(cmd, 0, 1, &scissor_);

    for (const auto& node : repository_->scene_graph.asVector()) {
      const AABB& bounds = node.second.get().bounds;
      AabbBvhPushConstants constants{glm::vec4(bounds.max, 0.f), glm::vec4(bounds.min, 0.f)};
      vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                         sizeof(AabbBvhPushConstants), &constants);
      vkCmdDraw(cmd, 36, 1, 0, 0);  // 6 vertices for the grid
    }

    vkCmdEndRendering(cmd);
  }

  void AabbBvhDebugPass::destroy() {}

  struct alignas(16) BoundingSpherePushConstants {
    glm::vec3 center;
    float radius;
  };

  void BoundingSphereDebugPass::prepare() {
    fmt::print("Preparing {}\n", get_name());

    dependencies_
        = RenderPassDependencyBuilder()
              .add_shader(ShaderStage::kVertex, "debug_bound_sphere.vert.spv")
              .add_shader(ShaderStage::kFragment, "debug_bound_sphere.frag.spv")
              .add_image_attachment(registry_->resources_.scene_color, ImageUsageType::kWrite, 1)
              .add_image_attachment(registry_->resources_.scene_depth,
                                    ImageUsageType::kDepthStencilRead, 1)
              .set_push_constant_range(sizeof(BoundingSpherePushConstants),
                                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
              .build();

    descriptor_layouts_.push_back(
        DescriptorLayoutBuilder()
            .add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT
                             | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_COMPUTE_BIT
                             | VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(gpu_->getDevice()));

    create_pipeline_layout();

    pipeline_ = create_pipeline()
                    .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                    .set_polygon_mode(VK_POLYGON_MODE_FILL)
                    .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                    .set_multisampling_none()
                    .enable_blending_additive()
                    .enable_depthtest(false, VK_COMPARE_OP_LESS_OR_EQUAL)
                    .build_graphics_pipeline(gpu_->getDevice());
  }

  void BoundingSphereDebugPass::execute(const VkCommandBuffer cmd) {
    if (!registry_->config_.debug_bounds_mesh) {
      return;
    }

    begin_renderpass(cmd);
    const auto frame = frame_->get_current_frame_index();
    const auto& per_frame_buffers = repository_->per_frame_data_buffers;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    bind_descriptor_buffers(cmd, {per_frame_buffers->descriptor_buffers[frame].get()});
    per_frame_buffers->descriptor_buffers[frame]->bind_descriptors(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdSetViewport(cmd, 0, 1, &viewport_);
    vkCmdSetScissor(cmd, 0, 1, &scissor_);

    for (const auto& draw : repository_->mesh_draws.data()) {
      BoundingSpherePushConstants constants{draw.center + draw.position, draw.radius * draw.scale};
      vkCmdPushConstants(cmd, pipeline_layout_,
                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(BoundingSpherePushConstants), &constants);
      vkCmdDraw(cmd, 36, 1, 0, 0);
    }

    vkCmdEndRendering(cmd);
  }

  void BoundingSphereDebugPass::destroy() {}
}  // namespace gestalt::graphics