#include "RenderPass.hpp"

#include <fmt/core.h>

namespace gestalt::graphics {
  RenderPass::RenderPass(std::string name): name_(std::move(name)) {
    fmt::println("Compiling Render Pass: {}", name_);
  }
}
