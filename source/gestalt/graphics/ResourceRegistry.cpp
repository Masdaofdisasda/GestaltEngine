#include "RenderPipeline.hpp"

#include <queue>

#include "Gui.hpp"
#include "RenderPass.hpp"
#include <VkBootstrap.h>

namespace gestalt::graphics {

    void ResourceRegistry::init(IGpu* gpu,
                                const Repository* repository) {
    gpu_ = gpu;

    attachment_list_.push_back(resources_.scene_color);
    attachment_list_.push_back(resources_.final_color);
    attachment_list_.push_back(resources_.scene_depth);
    attachment_list_.push_back(resources_.shadow_map);

    attachment_list_.push_back(resources_.gbuffer1);
    attachment_list_.push_back(resources_.gbuffer2);
    attachment_list_.push_back(resources_.gbuffer3);

    attachment_list_.push_back(resources_.bright_pass);
    attachment_list_.push_back(resources_.blur_y);

    attachment_list_.push_back(resources_.lum_64);
    attachment_list_.push_back(resources_.lum_32);
    attachment_list_.push_back(resources_.lum_16);
    attachment_list_.push_back(resources_.lum_8);
    attachment_list_.push_back(resources_.lum_4);
    attachment_list_.push_back(resources_.lum_2);
    attachment_list_.push_back(resources_.lum_1);

    attachment_list_.push_back(resources_.lum_A);
    attachment_list_.push_back(resources_.lum_B);

    resources_.IblBuffer.buffer
        = repository->ibl_buffers.get();
    resources_.perFrameDataBuffer.buffer
        = repository->per_frame_data_buffers.get();
    resources_.lightBuffer.buffer
        = repository->light_buffers.get();
    resources_.materialBuffer.buffer
        = repository->material_buffers.get();
    resources_.meshBuffer.buffer
        = repository->mesh_buffers.get();
  }

    VkShaderModule ResourceRegistry::get_shader(const ShaderProgram& shader_program) {
      const std::string& shader_path = "../shaders/" + shader_program.source_path;

      if (const auto it = shader_cache_.find(shader_path); it != shader_cache_.end()) {
        return it->second;
      }

      VkShaderModule shader_module;
      vkutil::load_shader_module(shader_path.c_str(), gpu_->getDevice(), &shader_module);

      shader_cache_[shader_path] = shader_module;

      return shader_module;
    }

    void ResourceRegistry::clear_shader_cache() {
      for (const auto& shader_module : shader_cache_ | std::views::values) {
        vkDestroyShaderModule(gpu_->getDevice(), shader_module, nullptr);
      }
      shader_cache_.clear();
    }

}  // namespace gestalt