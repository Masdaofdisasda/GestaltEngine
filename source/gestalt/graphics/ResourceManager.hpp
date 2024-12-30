#pragma once

#include "common.hpp"
#include "VulkanTypes.hpp"
#include "Interface/IResourceManager.hpp"

namespace gestalt::foundation {
  class Repository;
  class IGpu;
}

namespace gestalt::graphics {

    class ResourceManager final : public foundation::IResourceManager, public NonCopyable<ResourceManager> {

      IGpu* gpu_ = nullptr;
      Repository* repository_ = nullptr;

    public:

      void init(IGpu* gpu, Repository* repository) override;

      VkSampler create_sampler(const VkSamplerCreateInfo& sampler_create_info) const override;
      void generate_samplers() const;

    };
}  // namespace gestalt