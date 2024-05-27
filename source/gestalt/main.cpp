#include "graphics/GameEngine.hpp"

int main(int argc, char* argv[]) {

  gestalt::GameEngine engine;

  engine.init();

  engine.run();

  engine.cleanup();

  return 0;
}
