﻿#include "RenderPipeline.hpp"

#include <set>

#include <tracy/Tracy.hpp>

#include "Gui.hpp"
#include "RenderPass.hpp"
#include <VkBootstrap.h>

#include "vk_images.hpp"
#include "vk_initializers.hpp"
#include "vk_pipelines.hpp"

namespace gestalt::graphics {

    RenderPassDependencyBuilder& RenderPassDependencyBuilder::add_image_attachment(
        const ImageAttachment& attachment, const ImageUsageType usage,
        const uint32_t required_version, const ImageClearOperation clear_operation) {
      dependency_.image_attachments.push_back(
          {attachment, usage, clear_operation, required_version});
      return *this;
    }

    RenderPassDependencyBuilder& RenderPassDependencyBuilder::set_push_constant_range(uint32_t size,
        VkShaderStageFlags stage_flags) {
      dependency_.push_constant_range = {
          .stageFlags = stage_flags,
          .offset = 0,
          .size = size,
      };
      return *this;
    }

    RenderPassDependency RenderPassDependencyBuilder::build() { return dependency_; }

    void RenderPass::cleanup() {
      destroy();
      vkDestroyPipelineLayout(gpu_->getDevice(), pipeline_layout_, nullptr);
      vkDestroyPipeline(gpu_->getDevice(), pipeline_, nullptr);
    }

    void RenderPass::begin_renderpass(VkCommandBuffer cmd) {
      std::vector<VkRenderingAttachmentInfo> colorAttachments;
      VkRenderingAttachmentInfo depthAttachment = {};
      VkExtent2D extent = {0, 0};

      for (auto& attachment : dependencies_.image_attachments) {
        // only write and depth attachments are considered
        if (attachment.usage == ImageUsageType::kWrite
            || attachment.usage == ImageUsageType::kDepthStencilRead) {
          std::shared_ptr<TextureHandle>& image = attachment.attachment.image;

          // if the image is a color attachment
          if (image->getType() == TextureType::kColor) {
            if (extent.width == 0 && extent.height == 0) {
              extent = {image->imageExtent.width, image->imageExtent.height};
            }

            if (attachment.clear_operation == ImageClearOperation::kClear) {
              VkClearValue clear = {.color = {0.0f, 0.0f, 0.0f, 1.0f}};
              colorAttachments.push_back(
                  vkinit::attachment_info(image->imageView, &clear, image->getLayout()));
            } else {
              colorAttachments.push_back(
                  vkinit::attachment_info(image->imageView, nullptr, image->getLayout()));
            }

            // if the image is a depth attachment
          } else if (image->getType() == TextureType::kDepth) {
            if (extent.width == 0 && extent.height == 0) {
              extent = {image->imageExtent.width, image->imageExtent.height};
            }

            if (attachment.clear_operation == ImageClearOperation::kClear) {
              VkClearValue clear = {.depthStencil = {1.0f, 0}};
              depthAttachment
                  = vkinit::depth_attachment_info(image->imageView, &clear, image->getLayout());
            } else {
              depthAttachment
                  = vkinit::depth_attachment_info(image->imageView, nullptr, image->getLayout());
            }
          }
        }
      }

      viewport_.width = static_cast<float>(extent.width);
      viewport_.height = static_cast<float>(extent.height);
      scissor_.extent.width = extent.width;
      scissor_.extent.height = extent.height;

      VkRenderingInfo renderInfo;
      if (colorAttachments.empty()) {
        renderInfo = vkinit::rendering_info(extent, nullptr,
                                            depthAttachment.sType ? &depthAttachment : nullptr);
      } else {
        renderInfo = vkinit::rendering_info_for_gbuffer(
            extent, colorAttachments.data(), colorAttachments.size(),
            depthAttachment.sType ? &depthAttachment : nullptr);
      }

      vkCmdBeginRendering(cmd, &renderInfo);
    }

    PipelineBuilder RenderPass::create_pipeline() {
      VkShaderModule vertex_shader = VK_NULL_HANDLE;
      VkShaderModule fragment_shader = VK_NULL_HANDLE;
      VkShaderModule mesh_shader = VK_NULL_HANDLE;
      VkShaderModule task_shader = VK_NULL_HANDLE;
      VkShaderModule compute_shader = VK_NULL_HANDLE;

      for (auto& shader_dependency : dependencies_.shaders) {
        switch (shader_dependency.stage) {
          case ShaderStage::kVertex:
            vertex_shader = registry_->get_shader(shader_dependency);
            break;
          case ShaderStage::kFragment:
            fragment_shader = registry_->get_shader(shader_dependency);
            break;
          case ShaderStage::kMesh:
            mesh_shader = registry_->get_shader(shader_dependency);
            break;
          case ShaderStage::kTask:
            task_shader = registry_->get_shader(shader_dependency);
            break;
          case ShaderStage::kCompute:
                          compute_shader = registry_->get_shader(shader_dependency);
            break;
          default:
            break;
        }
      }

      auto builder = PipelineBuilder();

      if (vertex_shader != VK_NULL_HANDLE && fragment_shader != VK_NULL_HANDLE) {
        builder.set_shaders(vertex_shader, fragment_shader);
      } else if (mesh_shader != VK_NULL_HANDLE && task_shader != VK_NULL_HANDLE
                 && fragment_shader != VK_NULL_HANDLE) {
        builder.set_shaders(task_shader, mesh_shader, fragment_shader);
      } else if (compute_shader != VK_NULL_HANDLE) {
        builder.set_shaders(compute_shader);
      } else {
        throw std::runtime_error("Required shaders are not available for pipeline creation.");
      }

      // add color and depth attachments
      std::vector<VkFormat> color_formats;
      for (auto& attachment : dependencies_.image_attachments) {
        if (attachment.usage == ImageUsageType::kWrite
            || attachment.usage == ImageUsageType::kDepthStencilRead) {
          std::shared_ptr<TextureHandle>& image = attachment.attachment.image;

          if (image->getType() == TextureType::kColor) {
            color_formats.push_back(image->getFormat());
          } else if (image->getType() == TextureType::kDepth) {
            builder.set_depth_format(image->getFormat());
          }
        }
      }

      if (!color_formats.empty()) {
        if (color_formats.size() == 1) {  // single color attachment
          builder.set_color_attachment_format(color_formats[0]);
        } else {  // multiple color attachments
          builder.set_color_attachment_formats(color_formats);
        }
      }

      return builder.set_pipeline_layout(pipeline_layout_);
    }

    void RenderPass::create_pipeline_layout() {
      VkPipelineLayoutCreateInfo pipeline_layout_create_info{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
          .pNext = nullptr,
          .setLayoutCount = static_cast<uint32_t>(descriptor_layouts_.size()),
          .pSetLayouts = descriptor_layouts_.data(),
      };

      if (dependencies_.push_constant_range.size != 0) {
        pipeline_layout_create_info.pushConstantRangeCount = 1;
        pipeline_layout_create_info.pPushConstantRanges = &dependencies_.push_constant_range;
      }

      VK_CHECK(vkCreatePipelineLayout(gpu_->getDevice(), &pipeline_layout_create_info, nullptr,
                                      &pipeline_layout_));
    }

}  // namespace gestalt