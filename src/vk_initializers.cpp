#include <vk_initializers.hpp>

namespace vkinit {

VkCommandPoolCreateInfo commandPoolCreateInfo(std::uint32_t queueFamilyIndex,
                                              VkCommandPoolCreateFlags flags) {
  VkCommandPoolCreateInfo vkCommandPoolCreateInfo = {};
  vkCommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  vkCommandPoolCreateInfo.pNext = nullptr;
  vkCommandPoolCreateInfo.flags = flags;
  vkCommandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;

  return vkCommandPoolCreateInfo;
}

VkCommandBufferAllocateInfo
commandBufferAllocateInfo(VkCommandPool const pool, std::uint32_t const count,
                          VkCommandBufferLevel const level) {
  VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo;
  vkCommandBufferAllocateInfo.sType =
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  vkCommandBufferAllocateInfo.pNext = nullptr;
  vkCommandBufferAllocateInfo.commandPool = pool;
  vkCommandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  vkCommandBufferAllocateInfo.commandBufferCount = 1;

  return vkCommandBufferAllocateInfo;
}

} // namespace vkinit