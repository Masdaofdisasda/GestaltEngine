#include "pbr_pass.h"

#include "vk_initializers.h"
#include "vk_pipelines.h"
#include "vk_renderer.h"

void pbr_pass::init(vk_renderer& renderer) {
  gpu_ = renderer.gpu_;
  renderer_ = &renderer;

  {
    descriptor_layout_ = descriptor_layout_builder()
                             .add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                             .build(gpu_.device);
  }

  for (auto& frame : renderer_->frames_) {
    // create a descriptor pool
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
    };

    frame.descriptor_pools = DescriptorAllocatorGrowable{};
    frame.descriptor_pools.init(gpu_.device, 1000, frame_sizes);
  }

  VkShaderModule meshFragShader;
  vkutil::load_shader_module(fragment_shader_source_.c_str(), gpu_.device, &meshFragShader);

  VkShaderModule meshVertexShader;
  vkutil::load_shader_module(vertex_shader_source_.c_str(), gpu_.device, &meshVertexShader);

  VkPushConstantRange matrix_range{};
  matrix_range.offset = 0;
  matrix_range.size = sizeof(GPUDrawPushConstants);
  matrix_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkDescriptorSetLayout layouts[]
      = {descriptor_layout_, renderer_->resource_manager_->material_data.resource_layout,
         renderer_->resource_manager_->material_data.constants_layout,
         renderer_->resource_manager_->ibl_data.IblLayout};

  VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
  mesh_layout_info.setLayoutCount = 4;
  mesh_layout_info.pSetLayouts = layouts;
  mesh_layout_info.pPushConstantRanges = &matrix_range;
  mesh_layout_info.pushConstantRangeCount = 1;

  VkPipelineLayout newLayout;
  VK_CHECK(vkCreatePipelineLayout(gpu_.device, &mesh_layout_info, nullptr, &newLayout));

  PipelineBuilder pipelineBuilder;
  opaquePipelineLayout = newLayout;
  transparentPipelineLayout = newLayout;

  opaquePipeline
      = pipelineBuilder.set_shaders(meshVertexShader, meshFragShader)
            .set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .set_polygon_mode(VK_POLYGON_MODE_FILL)
            .set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
            .set_multisampling_none()
            .disable_blending()
            .enable_depthtest(true, VK_COMPARE_OP_LESS_OR_EQUAL)
            .set_color_attachment_format(renderer_->frame_buffer_.color_image.imageFormat)
            .set_depth_format(renderer_->frame_buffer_.depth_image.imageFormat)
            .set_pipeline_layout(newLayout)
            .build_pipeline(gpu_.device);

  // create the transparent variant
  transparentPipeline = pipelineBuilder.enable_blending_additive()
                            .enable_depthtest(false, VK_COMPARE_OP_LESS_OR_EQUAL)
                            .build_pipeline(gpu_.device);

  vkDestroyShaderModule(gpu_.device, meshFragShader, nullptr);
  vkDestroyShaderModule(gpu_.device, meshVertexShader, nullptr);
}

void pbr_pass::cleanup() {
  vkDestroyPipeline(gpu_.device, transparentPipeline, nullptr);
  vkDestroyPipeline(gpu_.device, opaquePipeline, nullptr);

  for (auto frame : renderer_->frames_) {
    frame.descriptor_pools.destroy_pools(gpu_.device);
  }
  vkDestroyDescriptorSetLayout(gpu_.device, descriptor_layout_, nullptr);
}

void pbr_pass::execute(VkCommandBuffer cmd) {
  std::vector<uint32_t> opaque_draws;
  opaque_draws.reserve(renderer_->main_draw_context_.opaque_surfaces.size());

  for (size_t i = 0; i < renderer_->main_draw_context_.opaque_surfaces.size(); i++) {
    // if (is_visible(main_draw_context_.opaque_surfaces[i], per_frame_data.viewproj)) {
    opaque_draws.push_back(i);
    //}
  }

  // sort the opaque surfaces by material and mesh
  std::ranges::sort(opaque_draws, [&](const auto& iA, const auto& iB) {
    const render_object& A = renderer_->main_draw_context_.opaque_surfaces[iA];
    const render_object& B = renderer_->main_draw_context_.opaque_surfaces[iB];
    if (A.material == B.material) {
      return A.first_index < B.first_index;
    }
    return A.material < B.material;
  });

  descriptor_set_
      = renderer_->get_current_frame().descriptor_pools.allocate(gpu_.device, descriptor_layout_);

  writer.clear();
  writer.write_buffer(0, renderer_->resource_manager_->per_frame_data_buffer.buffer,
                      sizeof(per_frame_data), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  writer.update_set(gpu_.device, descriptor_set_);

  vkCmdBindIndexBuffer(cmd, renderer_->resource_manager_->scene_geometry_.indexBuffer.buffer, 0,
                       VK_INDEX_TYPE_UINT32);

  auto draw = [&](const render_object& r, VkPipelineLayout pipeline_layout) {
    GPUDrawPushConstants push_constants;
    push_constants.worldMatrix = r.transform;
    push_constants.material_id = r.material * 5;
    push_constants.vertexBuffer = r.vertex_buffer_address;
    vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(GPUDrawPushConstants), &push_constants);

    renderer_->stats_.drawcall_count++;
    renderer_->stats_.triangle_count += r.index_count / 3;
    vkCmdDrawIndexed(cmd, r.index_count, 1, r.first_index, 0, 0);
  };

  // reset counters
  renderer_->stats_.drawcall_count = 0;
  renderer_->stats_.triangle_count = 0;

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, opaquePipeline);
  VkDescriptorSet descriptorSets[]
      = {descriptor_set_, renderer_->resource_manager_->material_data.resource_set,
         renderer_->resource_manager_->material_data.constants_set,
         renderer_->resource_manager_->ibl_data.IblSet};
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, opaquePipelineLayout, 0, 4,
                          descriptorSets, 0, nullptr);

  const VkViewport viewport = {
      .x = 0,
      .y = 0,
      .width = static_cast<float>(renderer_->window_.extent.width),
      .height = static_cast<float>(renderer_->window_.extent.height),
      .minDepth = 0.f,
      .maxDepth = 1.f,
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = {};
  scissor.offset.y = 0;
  scissor.offset.x = 0;
  scissor.extent.width = renderer_->window_.extent.width;
  scissor.extent.height = renderer_->window_.extent.height;
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  for (auto& r : opaque_draws) {
    draw(renderer_->main_draw_context_.opaque_surfaces[r], opaquePipelineLayout);
  }

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, transparentPipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, transparentPipelineLayout, 0, 3,
                          descriptorSets, 0, nullptr);

  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  for (auto& r : renderer_->main_draw_context_.transparent_surfaces) {
    draw(r, transparentPipelineLayout);
  }
}