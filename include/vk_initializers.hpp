#pragma once

#include <vk_types.hpp>

#include <cstdint>

namespace vkinit {

VkCommandPoolCreateInfo
commandPoolCreateInfo(std::uint32_t queueFamilyIndex,
                      VkCommandPoolCreateFlags flags = 0);

VkCommandBufferAllocateInfo commandBufferAllocateInfo(
    VkCommandPool pool, std::uint32_t count = 1,
    VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

} // namespace vkinit
