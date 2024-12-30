#pragma once
#include <Repository.hpp>

namespace gestalt::foundation {
  class IGpu;

  class IResourceManager {
  public:
    virtual ~IResourceManager() = default;

    virtual void init(IGpu* gpu, Repository* repository)
        = 0;


    virtual VkSampler create_sampler(const VkSamplerCreateInfo& sampler_create_info) const = 0;
  };
}  // namespace gestalt