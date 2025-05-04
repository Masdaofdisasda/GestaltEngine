#include "graphics/GameEngine.hpp"

int main(int argc, char* argv[]) {
  gestalt::foundation::EngineConfiguration::get_instance().load_from_file();
  auto config = gestalt::foundation::EngineConfiguration::get_instance().get_config();
  gestalt::foundation::EngineConfiguration::get_instance().set_config(config);

  gestalt::GameEngine engine;

  engine.run();

  return 0;
}
