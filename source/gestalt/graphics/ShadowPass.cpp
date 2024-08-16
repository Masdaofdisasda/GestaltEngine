#pragma once
#include "RenderPass.hpp"

#include "vk_images.hpp"
#include "vk_initializers.hpp"
#include "vk_pipelines.hpp"
#include <fmt/core.h>

#include "FrameProvider.hpp"


namespace gestalt::graphics {

  void DirectionalDepthPass::prepare() {
    fmt::print("Preparing {}\n", get_name());

    descriptor_layouts_.emplace_back(
        DescriptorLayoutBuilder()
            .add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT
                             | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_COMPUTE_BIT
                             | VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(gpu_->getDevice()));
    descriptor_layouts_.emplace_back(
        DescriptorLayoutBuilder()
            .add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, false,
                         getMaxDirectionalLights())
            .add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, false,
                         getMaxPointLights())
            .add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                         VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, false,
                         getMaxLights())
            .build(gpu_->getDevice()));
    constexpr auto mesh_stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT
                                 | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_COMPUTE_BIT;
    descriptor_layouts_.emplace_back(
        DescriptorLayoutBuilder()
            .add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
            .add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
            .add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
            .add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
            .add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
            .add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
            .add_binding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
            .add_binding(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mesh_stages)
            .build(gpu_->getDevice()));

    dependencies_
        = RenderPassDependencyBuilder()
              .add_shader(ShaderStage::kVertex, "shadow_geometry.vert.spv")
              .add_shader(ShaderStage::kFragment, "shadow_depth.frag.spv")
              .add_image_attachment(registry_->resources_.shadow_map, ImageUsageType::kWrite, 0,
                                    ImageClearOperation::kClear)
              .set_push_constant_range(sizeof(MeshletPushConstants), VK_SHADER_STAGE_VERTEX_BIT)
              .build();

    create_pipeline_layout();

    pipeline_ = create_pipeline()
                    .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                    .set_polygon_mode(VK_POLYGON_MODE_FILL)
                    .set_cull_mode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                    .set_multisampling_none()
                    .disable_blending()
                    .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
                    .build_graphics_pipeline(gpu_->getDevice());
  }

  void DirectionalDepthPass::destroy() {}

  void DirectionalDepthPass::execute(VkCommandBuffer cmd) {
    begin_renderpass(cmd);
    const auto frame = frame_->get_current_frame_index();

    const auto& per_frame_buffers = repository_->per_frame_data_buffers;
    const auto& light_data = repository_->light_buffers;
    const auto& mesh_buffers = repository_->mesh_buffers;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    bind_descriptor_buffers(cmd, {
        per_frame_buffers->descriptor_buffers[frame].get(),
        light_data->descriptor_buffer.get(),
        mesh_buffers->descriptor_buffers[frame].get(),
    });
    per_frame_buffers->descriptor_buffers[frame]->bind_descriptors(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                       pipeline_layout_, 0);
    light_data->descriptor_buffer->bind_descriptors(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 1);
    mesh_buffers->descriptor_buffers[frame]->bind_descriptors(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                  pipeline_layout_, 2);
    vkCmdSetViewport(cmd, 0, 1, &viewport_);
    vkCmdSetScissor(cmd, 0, 1, &scissor_);

    // draw all meshes

    vkCmdEndRendering(cmd);
  }
}  // namespace gestalt::graphics