#include "graphics/vk_engine.h"

int main(int argc, char* argv[]) {
  render_engine engine;

  engine.init();

  engine.run();

  engine.cleanup();

  return 0;
}
