#include <SDL.h>
#include <SDL_vulkan.h>
#include <vk_engine.hpp>

void VulkanEngine::init() {
  SDL_Init(SDL_INIT_VIDEO);
  SDL_WindowFlags windowFlags{static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN)};
  Window = SDL_CreateWindow("Vulkan Engine", SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED, WindowExtent.width,
                            WindowExtent.height, windowFlags);

  IsInitialized = true;
}

void VulkanEngine::run() {
  SDL_Event e;
  bool shouldQuit = false;

  while (!shouldQuit) {
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT)
        shouldQuit = true;
    }

    draw();
  }
}

void VulkanEngine::cleanup() {}

void VulkanEngine::draw() {}
