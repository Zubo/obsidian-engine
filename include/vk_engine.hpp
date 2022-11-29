#pragma once

#include <vk_types.hpp>

class VulkanEngine {
public:
  bool IsInitialized{false};
  int FrameNumber{0};
  VkExtent2D WindowExtent{1000, 800};

  struct SDL_Window *Window{nullptr};

  void init();
  void run();
  void cleanup();
  void draw();
};
