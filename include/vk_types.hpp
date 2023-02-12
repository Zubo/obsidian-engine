#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

struct AllocatedBuffer {
  VkBuffer _buffer;
  VmaAllocation _allocation;
};
