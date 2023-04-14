#include <vk_engine.hpp>

int main(int const argc, char const** argv) {
  VulkanEngine engine;

  engine.init();
  engine.run();
  engine.cleanup();

  return 0;
}