#pragma once
#include <string>
#include <vector>

#include "common.hpp"
#include "Utils/ResourceBindings.hpp"


namespace gestalt::graphics {
	
  class RenderPass : Moveable<RenderPass> {
    std::string name_;
  protected:
    explicit RenderPass(std::string name);

  public:
    [[nodiscard]] std::string_view get_name() const { return name_; }
    virtual std::vector<ResourceBinding<ResourceInstance>> get_resources(ResourceUsage usage) = 0;
    virtual std::map<uint32, std::shared_ptr<DescriptorBufferInstance>> get_descriptor_buffers()
        = 0; 
    virtual void execute(CommandBuffer cmd) = 0;
    virtual ~RenderPass() = default;
  };

}
