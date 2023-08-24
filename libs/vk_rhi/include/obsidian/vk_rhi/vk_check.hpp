#pragma once

#include <obsidian/core/logging.hpp>
#include <vulkan/vulkan.h>

#include <cstdlib>
#include <string>

#define VK_CHECK(x)                                                            \
  do {                                                                         \
    VkResult err = x;                                                          \
    if (err) {                                                                 \
      OBS_LOG_ERR("Detected Vulkan error: " + std::to_string(err));            \
      std::abort();                                                            \
    }                                                                          \
  } while (0)
