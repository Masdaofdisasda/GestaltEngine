

#include "Pipeline.hpp"

#include "common.hpp"
#include "VulkanCheck.hpp"

namespace gestalt::graphics::fg {
  void PipelineTool::create_descriptor_layout(
      std::unordered_map<uint32, std::unordered_map<uint32, VkDescriptorSetLayoutBinding>>&&
      sets) {
    this->set_bindings_ = std::move(sets);

    // pipeline layout expects descriptor set layouts to be sorted by set index
    std::vector<uint32_t> sorted_keys;
    sorted_keys.reserve(set_bindings_.size());
    for (const auto& set_index : set_bindings_ | std::views::keys) {
      sorted_keys.push_back(set_index);
    }
    std::ranges::sort(sorted_keys);

    for (const auto& set_index : sorted_keys) {
      const auto& bindings = set_bindings_.at(set_index);
      std::vector<VkDescriptorSetLayoutBinding> binding_vector;
      binding_vector.reserve(bindings.size());
      for (const auto& binding : bindings | std::views::values) {
        binding_vector.push_back(binding);
      }

      VkDescriptorSetLayoutCreateInfo info
          = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
             .pNext = nullptr,
             .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
             .bindingCount = static_cast<uint32>(binding_vector.size()),
             .pBindings = binding_vector.data()};

      VkDescriptorSetLayout descriptor_set_layout;
      VK_CHECK(
          vkCreateDescriptorSetLayout(gpu_->getDevice(), &info, nullptr, &descriptor_set_layout));
      const std::string descriptor_set_layout_name = std::string(pipeline_name_) + " Set "
                                                     + std::to_string(set_index)
                                                     + " Descriptor Set Layout";
      gpu_->set_debug_name(descriptor_set_layout_name, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                           reinterpret_cast<uint64_t>(descriptor_set_layout));
      descriptor_set_layouts_.emplace(set_index, descriptor_set_layout);
    }
  }
}
