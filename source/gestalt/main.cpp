#include "graphics/RenderEngine.h"

int main(int argc, char* argv[]) {
  RenderEngine engine;

  engine.init();

  engine.run();

  engine.cleanup();

  return 0;
}
