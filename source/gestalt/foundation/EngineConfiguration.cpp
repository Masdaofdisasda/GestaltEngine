#include "EngineConfiguration.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

#include "fmt/compile.h"

namespace gestalt::foundation {
  EngineConfiguration& EngineConfiguration::get_instance() {
    static EngineConfiguration instance;
    return instance;
  }

  void EngineConfiguration::load_from_file(const std::string& filename) {
    std::ifstream config_file(filename);

    // If the file doesn't exist, create it with default values
    if (!config_file) {
      fmt::println("Configuration file not found, creating default configuration: {}", filename);

      nlohmann::json config_json = {
                                    {"applicationName", config_.applicationName},
                                    {"windowedWidth", config_.windowedWidth},
                                    {"windowedHeight", config_.windowedHeight},
                                    {"useFullscreen", config_.useFullscreen},
                                    {"useVsync", config_.useVsync},
                                    {"useValidationLayers", config_.useValidationLayers}};

      std::ofstream out_config_file(filename);
      if (out_config_file) {
        out_config_file << config_json.dump(4);  // Pretty print with 4-space indentation
        fmt::println("Default configuration saved to: {}", filename);
      } else {
        fmt::println("Error: Could not create configuration file: {}", filename);
      }
      return;
    }

    // If the file exists, load the configuration from it
    if (!config_file) {
      fmt::println("Could not open configuration file: {}", filename);
      return;
    }

    // Parse the JSON configuration file
    nlohmann::json config_json;
    try {
      config_file >> config_json;
    } catch (const nlohmann::json::parse_error& e) {
      fmt::println("JSON parse error in configuration file: {}", e.what());
      return;
    }

    try {
      config_.applicationName = config_json.value("applicationName", config_.applicationName);
      config_.windowedWidth = config_json.value("windowedWidth", config_.windowedWidth);
      config_.windowedHeight = config_json.value("windowedHeight", config_.windowedHeight);

      config_.useFullscreen = config_json.value("useFullscreen", config_.useFullscreen);
      config_.useVsync = config_json.value("useVsync", config_.useVsync);
      config_.useValidationLayers
          = config_json.value("useValidationLayers", config_.useValidationLayers);

    } catch (const nlohmann::json::type_error& e) {
      fmt::println("JSON type error in configuration file: {}", e.what());
    }
  }
}  // namespace gestalt::foundation