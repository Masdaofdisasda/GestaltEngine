#pragma once

#include "vk_types.h"
#include "vk_gpu.h"
#include "resource_manager.h"

class shader_resource {
public:
  virtual ~shader_resource() = default;

  virtual std::string getId() const = 0;
};

class color_image_resource final : public shader_resource {
  std::string id;
  float scale_ = 1.0f;
  VkExtent2D extent_ = {0,0};

  std::shared_ptr<AllocatedImage> image_;

public:
  color_image_resource(std::string id, VkExtent2D extent)
      : id(std::move(id)), extent_(extent) {}

  color_image_resource(std::string id, const float scale) : id(std::move(id)), scale_(scale) {}

  std::string getId() const override { return id; }
  float get_scale() const { return scale_; }
  VkExtent2D get_extent() const { return extent_; }
  void set_image(const std::shared_ptr<AllocatedImage>& image) { image_ = image; }
};

class depth_image_resource final : public shader_resource {
  std::string id;
  float scale_ = 1.0f;
  VkExtent2D extent_ = {0, 0};

  std::shared_ptr<AllocatedImage> image_;

public:
  depth_image_resource(std::string id, VkExtent2D extent) : id(std::move(id)), extent_(extent) {}
  depth_image_resource(std::string id, const float scale)
      : id(std::move(id)), scale_(scale) {}

  std::string getId() const override { return id; }
  float get_scale() const { return scale_; }
  VkExtent2D get_extent() const { return extent_; }
  void set_image(const std::shared_ptr<AllocatedImage>& image) { image_ = image; }
};

class buffer_resource final : public shader_resource {
  std::string id;
  VkDeviceSize size;

public:
  buffer_resource(const std::string& id, VkDeviceSize size) : id(id), size(size) {}

  std::string getId() const override { return id; }
  VkDeviceSize get_size() const { return size; }
};

struct shader_pass_dependency_info {
  std::vector<std::shared_ptr<shader_resource>> read_resources;
  std::vector<std::pair<std::string, std::shared_ptr<shader_resource>>> write_resources;
};

class shader_pass {
  public:
  void init(const vk_gpu& gpu,
                 const std::shared_ptr<resource_manager>& resource_manager) {
       gpu_ = gpu;
    resource_manager_ = resource_manager;

    prepare();
  }
  virtual ~shader_pass() = default;

  virtual void execute(VkCommandBuffer cmd) = 0;
  virtual shader_pass_dependency_info& get_dependencies() = 0;
  virtual void cleanup() = 0;

protected:
  virtual void prepare() = 0;

  vk_gpu gpu_ = {};
  std::shared_ptr<resource_manager> resource_manager_;
};