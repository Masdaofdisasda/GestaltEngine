#pragma once

#include "common.hpp"
#include <string>

namespace gestalt::foundation {

  // Compile time configuration
  constexpr uint32 kDefaultFramesInFlight = 2;
  constexpr uint32 kDefaultMaxMaterials = 256;
  constexpr uint32 kDefaultPbrMaterialTextures = 5;  // albedo, normal, metallic, roughness, ao
  constexpr uint32 kDefaultMaxTextures = kDefaultMaxMaterials * kDefaultPbrMaterialTextures;

  // should be enough for Bistro scene
  constexpr uint32 kDefaultMaxVertices = 8388608;
  constexpr uint32 kDefaultMaxIndices = 2 * kDefaultMaxVertices;
  constexpr uint32 kDefaultMaxMeshes = 4096;
  constexpr uint32 kDefaultMaxMeshlets = 131072;

  constexpr uint32 kDefaultMaxDirectionalLights = 2;  // needed for 64 bit alignment
  constexpr uint32 kDefaultMaxPointLights = 256;
  constexpr uint32 kDefaultMaxSpotLights = 0;  // are not implemented yet

  // Run time configuration
  constexpr std::string_view kDefaultApplicationName = "Gestalt Engine";
  constexpr uint32 kDefaultResolutionWidth = 1300;
  constexpr uint32 kDefaultResolutionHeight = 900;
  constexpr bool kUseFullscreen = false;
  constexpr bool kUseVsync = false;
  constexpr bool kUseValidationLayers = false;

  struct Config {
    // compile time configuration
    uint32 max_directional_lights = kDefaultMaxDirectionalLights;
    uint32 maxPointLights = kDefaultMaxPointLights;
    uint32 maxSpotLights = kDefaultMaxSpotLights;
    uint32 maxLights = max_directional_lights + maxPointLights + maxSpotLights;

    // run time configuration
    std::string applicationName = std::string(kDefaultApplicationName);
    uint32 windowedWidth = kDefaultResolutionWidth;
    uint32 windowedHeight = kDefaultResolutionHeight;
    bool useFullscreen = kUseFullscreen;
    bool useValidationLayers = kUseValidationLayers;
    bool useVsync = kUseVsync;
  };

  class EngineConfiguration {
  public:
    static EngineConfiguration& getInstance();

    void loadFromFile(const std::string& filename = "../config.json");

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

  constexpr uint32 getMaxMeshes() { return kDefaultMaxMeshes; }

  constexpr uint32 getMaxMeshlets() { return kDefaultMaxMeshlets; }

  inline uint32 getMaxDirectionalLights() {
    return EngineConfiguration::getInstance().getConfig().max_directional_lights;
  }

  inline uint32 getMaxPointLights() {
    return EngineConfiguration::getInstance().getConfig().maxPointLights;
  }

  inline uint32 getMaxSpotLights() {
    return EngineConfiguration::getInstance().getConfig().maxSpotLights;
  }

  inline uint32 getMaxLights() { return EngineConfiguration::getInstance().getConfig().maxLights; }

  inline uint32 getWindowedWidth() {
    return EngineConfiguration::getInstance().getConfig().windowedWidth;
  }

  inline uint32 getWindowedHeight() {
    return EngineConfiguration::getInstance().getConfig().windowedHeight;
  }

  inline std::string& getApplicationName() {
    return EngineConfiguration::getInstance().getConfig().applicationName;
  }

  inline bool useValidationLayers() {
    return EngineConfiguration::getInstance().getConfig().useValidationLayers;
  }

  inline bool useVsync() { return EngineConfiguration::getInstance().getConfig().useVsync; }

  inline bool useFullscreen() {
    return EngineConfiguration::getInstance().getConfig().useFullscreen;
  }

}  // namespace gestalt::foundation