#pragma once

#include "common.hpp"

namespace gestalt::foundation {

  // Compile time configuration
  constexpr uint32 kDefaultFramesInFlight = 2;
  constexpr uint32 kDefaultMaxMaterials = 256;
  constexpr uint32 kDefaultPbrMaterialTextures = 5;  // albedo, normal, metallic, roughness, ao
  constexpr uint32 kDefaultMaxTextures = kDefaultMaxMaterials * kDefaultPbrMaterialTextures;

  // should be enough for Bistro scene
  constexpr uint32 kDefaultMaxVertices = 32768;   // 2^15
  constexpr uint32 kDefaultMaxIndices = 1048576;  // 2^20

  // Run time configuration
  constexpr uint32 kDefaultMaxDirectionalLights = 2;  // needed for 64 bit alignment
  constexpr uint32 kDefaultMaxPointLights = 256;
  constexpr uint32 kDefaultMaxSpotLights = 0;  // are not implemented yet

  constexpr uint32 kDefaultResolutionWidth = 1300;
  constexpr uint32 kDefaultResolutionHeight = 900;

  constexpr bool kUseValidationLayers = false;

  struct Config {
    uint32 maxDirectionalLights = kDefaultMaxDirectionalLights;
    uint32 maxPointLights = kDefaultMaxPointLights;
    uint32 maxSpotLights = kDefaultMaxSpotLights;
    uint32 maxLights = maxDirectionalLights + maxPointLights + maxSpotLights;

    uint32 resolutionWidth = kDefaultResolutionWidth;
    uint32 resolutionHeight = kDefaultResolutionHeight;

    bool useValidationLayers = kUseValidationLayers;
  };

  class EngineConfiguration {
  public:
    static EngineConfiguration& getInstance() {
      static EngineConfiguration instance;
      return instance;
    }

    Config& getConfig() { return config_; }

    void setConfig(const Config& newConfig) { config_ = newConfig; }

    EngineConfiguration(const EngineConfiguration&) = delete;
    EngineConfiguration& operator=(const EngineConfiguration&) = delete;

  private:
    Config config_{};
    EngineConfiguration() = default;
    ~EngineConfiguration() = default;
  };

  constexpr uint32 getFramesInFlight() { return kDefaultFramesInFlight; }

  constexpr uint32 getMaxMaterials() { return kDefaultMaxMaterials; }

  constexpr uint32 getPbrMaterialTextures() { return kDefaultPbrMaterialTextures; }

  constexpr uint32 getMaxTextures() { return kDefaultMaxTextures; }

  constexpr uint32 getMaxVertices() { return kDefaultMaxVertices; }

  constexpr uint32 getMaxIndices() { return kDefaultMaxIndices; }

  inline uint32 getMaxDirectionalLights() {
    return EngineConfiguration::getInstance().getConfig().maxDirectionalLights;
  }

  inline uint32 getMaxPointLights() {
    return EngineConfiguration::getInstance().getConfig().maxPointLights;
  }

  inline uint32 getMaxSpotLights() {
    return EngineConfiguration::getInstance().getConfig().maxSpotLights;
  }

  inline uint32 getMaxLights() { return EngineConfiguration::getInstance().getConfig().maxLights; }

  inline uint32 getResolutionWidth() {
    return EngineConfiguration::getInstance().getConfig().resolutionWidth;
  }

  inline uint32 getResolutionHeight() {
    return EngineConfiguration::getInstance().getConfig().resolutionHeight;
  }

  inline bool useValidationLayers() {
    return EngineConfiguration::getInstance().getConfig().useValidationLayers;
  }

}  // namespace gestalt::foundation