#include "graphics/Engine.hpp"

int main(int argc, char* argv[]) {

  gestalt::Engine engine;

  engine.init();

  engine.run();

  engine.cleanup();

  return 0;
}
