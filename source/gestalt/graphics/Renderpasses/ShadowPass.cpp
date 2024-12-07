#include "Renderpasses/RenderPassTypes.hpp"

#include <fmt/core.h>

#include "FrameProvider.hpp"
#include "Repository.hpp"
#include "Interface/IGpu.hpp"
#include "Render Engine/ResourceRegistry.hpp"
#include "Utils/vk_descriptors.hpp"


namespace gestalt::graphics {

  struct alignas(16) DrawCullDepthConstants {
    int32 draw_count;
  };

  void DrawCullDirectionalDepthPass::prepare() {
    fmt::print("Preparing {}\n", get_name());

    descriptor_layouts_.push_back(
        DescriptorLayoutBuilder()
            .add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT
                             | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_COMPUTE_BIT
                             | VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(gpu_->getDevice()));

    constexpr auto mesh_stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT
                                 | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_COMPUTE_BIT;
    descriptor_layouts_.push_back(
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
              .add_shader(ShaderStage::kCompute, "draw_cull_depth.comp.spv")
              .add_buffer_dependency(registry_->resources_.perFrameDataBuffer,
                                     BufferUsageType::kRead, 0)
              .add_buffer_dependency(registry_->resources_.meshBuffer, BufferUsageType::kWrite, 1)
              .set_push_constant_range(sizeof(DrawCullDepthConstants), VK_SHADER_STAGE_COMPUTE_BIT)
              .build();

    create_pipeline_layout();

    pipeline_ = create_pipeline().build_compute_pipeline(gpu_->getDevice());
  }

  void DrawCullDirectionalDepthPass::destroy() {}

    inline glm::vec4 NormalizePlane(const glm::vec4 p) { return p / length(glm::vec3(p)); }

  void DrawCullDirectionalDepthPass::execute(VkCommandBuffer cmd) {
    const auto frame = frame_->get_current_frame_index();

    const auto& mesh_buffers = repository_->mesh_buffers;

    vkCmdFillBuffer(cmd, mesh_buffers->draw_count_buffer[frame]->get_buffer_handle(), 0,
                    mesh_buffers->draw_count_buffer[frame]->get_size(), 0);

    const int32 maxCommandCount = repository_->mesh_draws.size();  // each mesh gets a draw command
    const uint32 groupCount
        = (static_cast<uint32>(maxCommandCount) + 63) / 64;  // 64 threads per group

    DrawCullDepthConstants draw_cull_constants{.draw_count = maxCommandCount};

    begin_compute_pass(cmd);

    vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(DrawCullDepthConstants), &draw_cull_constants);
    vkCmdDispatch(cmd, groupCount, 1, 1);

    // Memory barrier to ensure writes are visible to the second compute shader
    VkMemoryBarrier memoryBarrier = {};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0,
                         nullptr);
  }

  void TaskSubmitDirectionalDepthPass::prepare() {
    fmt::print("Preparing {}\n", get_name());

    constexpr auto mesh_stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT
                                 | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_COMPUTE_BIT;
    descriptor_layouts_.push_back(
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
              .add_shader(ShaderStage::kCompute, "task_submit.comp.spv")
              .add_buffer_dependency(registry_->resources_.meshBuffer, BufferUsageType::kWrite, 0)
              .build();

    create_pipeline_layout();

    pipeline_ = create_pipeline().build_compute_pipeline(gpu_->getDevice());
  }

  void TaskSubmitDirectionalDepthPass::destroy() {}

  void TaskSubmitDirectionalDepthPass::execute(VkCommandBuffer cmd) {
    begin_compute_pass(cmd);
    vkCmdDispatch(cmd, 1, 1, 1);

    VkMemoryBarrier memoryBarrier = {};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;  // Compute shader writes data
    memoryBarrier.dstAccessMask
        = VK_ACCESS_SHADER_READ_BIT
          | VK_ACCESS_SHADER_WRITE_BIT;  // Mesh/Task shaders read/write data

    // Pipeline barrier between compute and task/mesh shaders
    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // Source stage: compute shader
        VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT
            | VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT,  // Destination stages: task/mesh shaders
        0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

  }

  struct alignas(16) MeshletDepthPushConstants {
    int cullFlags{0};
    float32 pyramidWidth, pyramidHeight;  // depth pyramid size in texels
    int32 clusterOcclusionEnabled;
  };

  void MeshletDirectionalDepthPass::prepare() {
    fmt::print("Preparing {}\n", get_name());

    descriptor_layouts_.push_back(
        DescriptorLayoutBuilder()
            .add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                         VK_SHADER_STAGE_MESH_BIT_EXT
                             | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_COMPUTE_BIT
                             | VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(gpu_->getDevice()));
    descriptor_layouts_.emplace_back(
        DescriptorLayoutBuilder()
            .add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_MESH_BIT_EXT)
            .add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_MESH_BIT_EXT)
            .add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                         VK_SHADER_STAGE_MESH_BIT_EXT)
            .build(gpu_->getDevice()));
    constexpr auto mesh_stages = VK_SHADER_STAGE_MESH_BIT_EXT
                                 | VK_SHADER_STAGE_TASK_BIT_EXT;
    descriptor_layouts_.push_back(
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
              .add_shader(ShaderStage::kTask, "geometry_depth.task.spv")
              .add_shader(ShaderStage::kMesh, "geometry_depth.mesh.spv")
              .add_shader(ShaderStage::kFragment, "geometry_depth.frag.spv")
              .add_image_attachment(registry_->resources_.shadow_map, ImageUsageType::kWrite, 0,
                                    ImageClearOperation::kClear)
              .add_buffer_dependency(registry_->resources_.perFrameDataBuffer,
                                     BufferUsageType::kRead, 0)
              .add_buffer_dependency(registry_->resources_.lightBuffer, BufferUsageType::kRead, 1)
              .add_buffer_dependency(registry_->resources_.meshBuffer, BufferUsageType::kRead, 2)
              .set_push_constant_range(sizeof(MeshletDepthPushConstants),
                                       VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT)
              .build();

    create_pipeline_layout();

    pipeline_ = create_pipeline()
                    .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                    .set_polygon_mode(VK_POLYGON_MODE_FILL)
                    .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                    .set_multisampling_none()
                    .disable_blending()
                    .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
                    .build_graphics_pipeline(gpu_->getDevice());
  }

  void MeshletDirectionalDepthPass::destroy() {}

  void MeshletDirectionalDepthPass::execute(VkCommandBuffer cmd) {
    begin_renderpass(cmd);

    const auto frame = frame_->get_current_frame_index();
    const auto& mesh_buffers = repository_->mesh_buffers;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdSetViewport(cmd, 0, 1, &viewport_);
    vkCmdSetScissor(cmd, 0, 1, &scissor_);

    MeshletDepthPushConstants push_constants;
    push_constants.clusterOcclusionEnabled = meshlet_push_constants.clusterOcclusionEnabled;
    push_constants.pyramidWidth = meshlet_push_constants.pyramidWidth;
    push_constants.pyramidHeight = meshlet_push_constants.pyramidHeight;
    push_constants.cullFlags = meshlet_push_constants.cullFlags;

    vkCmdPushConstants(cmd, pipeline_layout_,
                       VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT, 0,
                       sizeof(MeshletDepthPushConstants), &push_constants);

    // first byte is the task count, so we need offset by one uint32
    vkCmdDrawMeshTasksIndirectEXT(cmd, mesh_buffers->draw_count_buffer[frame]->get_buffer_handle(),
                                  sizeof(uint32), 1, 0);

    vkCmdEndRendering(cmd);

    VkMemoryBarrier memoryBarrier = {};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;  // Compute shader writes data
    memoryBarrier.dstAccessMask
        = VK_ACCESS_SHADER_READ_BIT
          | VK_ACCESS_SHADER_WRITE_BIT;  // Mesh/Task shaders read/write data

    // Pipeline barrier between compute and task/mesh shaders
    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT
            | VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
        0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

  }
}  // namespace gestalt::graphics