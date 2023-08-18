#include <rhi/rhi.hpp>

int main(int const argc, char const** argv) {
  obsidian::rhi::RHI engine;

  engine.init();
  engine.run();
  engine.cleanup();

  return 0;
}