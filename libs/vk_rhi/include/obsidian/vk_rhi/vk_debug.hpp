#pragma once

#include <obsidian/vk_rhi/vk_framebuffer.hpp>

#include <vulkan/vulkan.h>

#include <cstdint>

namespace obsidian::vk_rhi {

void initDebugUtils(VkInstance vkInstance);

void setDbgResourceName(VkDevice vkDevice, std::uint64_t objHandle,
                        VkObjectType objType, char const* objName,
                        char const* additionalInfo = nullptr);

void nameFramebufferResources(VkDevice vkDevice, Framebuffer const& framebuffer,
                              char const* framebufferName);

} /*namespace obsidian::vk_rhi*/
