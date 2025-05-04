

#include "Pipeline.hpp"

#include "common.hpp"
#include "VulkanCheck.hpp"
#include "fmt/core.h"

namespace gestalt::graphics {
  void PipelineTool::create_descriptor_layout(
      std::map<uint32, std::map<uint32, VkDescriptorSetLayoutBinding>>&&
      sets) {
    this->set_bindings_ = std::move(sets);

    for (const auto& [set_index, bindings ] : set_bindings_) {
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

  VkShaderModule PipelineTool::load_shader_module(const std::filesystem::path& file_path,
      const VkDevice device) {
    // 1. Open the file in binary mode, cursor at the end
    std::ifstream file(file_path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
      throw std::runtime_error(
          fmt::format("Error: Could not open shader file: {}", file_path.string()));
    }

    // 2. Get the file size in bytes
    const auto fileSize = file.tellg();
    if (fileSize < 0) {
      throw std::runtime_error(
          fmt::format("Error: Failed to read file size: {}", file_path.string()));
    }

    // 3. Create a buffer big enough for the entire file
    std::vector<std::uint32_t> buffer(fileSize / sizeof(std::uint32_t));

    // 4. Reset cursor to beginning and read the file into the buffer
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    file.close();

    // 5. Set up the shader module create info
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.codeSize = buffer.size() * sizeof(std::uint32_t);
    createInfo.pCode = buffer.data();

    // 6. Create the shader module
    VkShaderModule shaderModule{};
    if (const VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
      result != VK_SUCCESS) {
      throw std::runtime_error(
          fmt::format("Error: Failed to create shader module for file: {}", file_path.string()));
    }

    return shaderModule;
  }
}
