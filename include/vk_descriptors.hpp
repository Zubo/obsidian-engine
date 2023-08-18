#pragma once

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

class DescriptorAllocator {
public:
  void init(VkDevice vkDevice, VkDescriptorPoolCreateFlags vkFlags);
  void cleanup();
  bool allocate(VkDescriptorSetLayout const& vkDescriptorSetLayout,
                VkDescriptorSet* outDescriptorSet);
  void resetPools();

private:
  VkDescriptorPool getAvailableDescriptorPool();
  VkDescriptorPool createNewDescriptorPool();

  VkDevice _vkDevice;
  VkDescriptorPoolCreateFlags _vkDescriptorPoolCreateFlags = 0;
  VkDescriptorPool _vkCurrentDescriptorPool = VK_NULL_HANDLE;
  std::vector<VkDescriptorPool> _usedDescriptorPools;
  std::vector<VkDescriptorPool> _availableDescriptorPools;
  std::size_t _descriptorPoolSizeFactor = 1;

  static constexpr std::uint32_t maxSets = 1000u;
  static constexpr VkDescriptorPoolSize descriptorPoolSizes[]{
      {VK_DESCRIPTOR_TYPE_SAMPLER, 50},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 400},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 400},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 200},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 200},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 50}};
};

class DescriptorLayoutCache {
public:
  void init(VkDevice vkDevice);
  void cleanup();
  VkDescriptorSetLayout getLayout(
      VkDescriptorSetLayoutCreateInfo const& descriptorSetLayoutCreateInfo);

private:
  struct DescriptorLayoutInfo {
    std::vector<VkDescriptorSetLayoutBinding> descriptorLayoutBindings;
    VkDescriptorSetLayoutCreateFlags flags;

    bool operator==(DescriptorLayoutInfo const& other) const;
  };

  struct DescriptorLayoutInfoHash {
    std::size_t
    operator()(DescriptorLayoutInfo const& descriptorLayoutInfo) const;
  };

  VkDevice _vkDevice;
  std::unordered_map<DescriptorLayoutInfo, VkDescriptorSetLayout,
                     DescriptorLayoutInfoHash>
      _descriptorSetLayoutMap;
};

class DescriptorBuilder {};
