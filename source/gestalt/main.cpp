#include "graphics/RenderEngine.h"

int main(int argc, char* argv[]) {

  using namespace gestalt;

  RenderEngine engine;

  engine.init();

  engine.run();

  engine.cleanup();

  return 0;
}
