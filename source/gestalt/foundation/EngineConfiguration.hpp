#pragma once

#include "common.hpp"
#include <string>

namespace gestalt::foundation {

  // Compile time configuration
  constexpr uint32 kDefaultFramesInFlight = 1;
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
  constexpr uint32 kDefaultMaxSpotLights = 256;

  // Run time configuration
  constexpr std::string_view kDefaultApplicationName = "Gestalt Engine";
  constexpr std::string_view kDefaultScene = "";
  constexpr uint32 kDefaultResolutionWidth = 1300;
  constexpr uint32 kDefaultResolutionHeight = 900;
  constexpr bool kUseFullscreen = false;
  constexpr bool kUseVsync = false;
  constexpr bool kUseValidationLayers = false;
  constexpr bool kDefaultEnableVulkanRayTracing = true;

  struct Config {
    // compile time configuration
    uint32 max_directional_lights = kDefaultMaxDirectionalLights;
    uint32 maxPointLights = kDefaultMaxPointLights;
    uint32 maxSpotLights = kDefaultMaxSpotLights;
    uint32 maxLights = max_directional_lights + maxPointLights + maxSpotLights;

    // run time configuration
    std::string applicationName = std::string(kDefaultApplicationName);
    std::string initialScene = std::string(kDefaultScene);
    uint32 windowedWidth = kDefaultResolutionWidth;
    uint32 windowedHeight = kDefaultResolutionHeight;
    bool useFullscreen = kUseFullscreen;
    bool useValidationLayers = kUseValidationLayers;
    bool useVsync = kUseVsync;
    bool enableVulkanRayTracing = kDefaultEnableVulkanRayTracing;
    uint32 physicalDeviceIndex = 0;
  };

  class EngineConfiguration {
  public:
    static EngineConfiguration& get_instance();

    void load_from_file(const std::string& filename = "../config.json");

    Config& get_config() { return config_; }

    void set_config(const Config& new_config) { config_ = new_config; }

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
    return EngineConfiguration::get_instance().get_config().max_directional_lights;
  }

  inline uint32 getMaxPointLights() {
    return EngineConfiguration::get_instance().get_config().maxPointLights;
  }

  inline uint32 getMaxSpotLights() {
    return EngineConfiguration::get_instance().get_config().maxSpotLights;
  }

  inline uint32 getMaxLights() { return EngineConfiguration::get_instance().get_config().maxLights; }

  inline uint32 getWindowedWidth() {
    return EngineConfiguration::get_instance().get_config().windowedWidth;
  }

  inline uint32 getWindowedHeight() {
    return EngineConfiguration::get_instance().get_config().windowedHeight;
  }

  inline std::string& getApplicationName() {
    return EngineConfiguration::get_instance().get_config().applicationName;
  }

  inline std::string& getInitialScene() {
    return EngineConfiguration::get_instance().get_config().initialScene;
  }

  inline bool useValidationLayers() {
    return EngineConfiguration::get_instance().get_config().useValidationLayers;
  }

  inline bool useVsync() { return EngineConfiguration::get_instance().get_config().useVsync; }

  inline bool useFullscreen() {
    return EngineConfiguration::get_instance().get_config().useFullscreen;
  }
  inline bool isVulkanRayTracingEnabled() {
    return EngineConfiguration::get_instance().get_config().enableVulkanRayTracing;
  }
  inline uint32 getPhysicalDeviceIndex() {
    return EngineConfiguration::get_instance().get_config().physicalDeviceIndex;
  }
}  // namespace gestalt::foundation