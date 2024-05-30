#include "RenderPipeline.hpp"

#include <queue>

#include "Gui.hpp"
#include "RenderPass.hpp"
#include <VkBootstrap.h>

namespace gestalt::graphics {

    void ResourceRegistry::init(const std::shared_ptr<IGpu>& gpu) {
      gpu_ = gpu;

      attachment_list_.push_back(attachments_.scene_color);
      attachment_list_.push_back(attachments_.final_color);
      attachment_list_.push_back(attachments_.scene_depth);
      attachment_list_.push_back(attachments_.shadow_map);

      attachment_list_.push_back(attachments_.gbuffer1);
      attachment_list_.push_back(attachments_.gbuffer2);
      attachment_list_.push_back(attachments_.gbuffer3);

      attachment_list_.push_back(attachments_.bright_pass);
      attachment_list_.push_back(attachments_.blur_y);

      attachment_list_.push_back(attachments_.lum_64);
      attachment_list_.push_back(attachments_.lum_32);
      attachment_list_.push_back(attachments_.lum_16);
      attachment_list_.push_back(attachments_.lum_8);
      attachment_list_.push_back(attachments_.lum_4);
      attachment_list_.push_back(attachments_.lum_2);
      attachment_list_.push_back(attachments_.lum_1);

      attachment_list_.push_back(attachments_.lum_A);
      attachment_list_.push_back(attachments_.lum_B);
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