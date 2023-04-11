#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

struct AllocatedBuffer {
  VkBuffer buffer;
  VmaAllocation allocation;
};
