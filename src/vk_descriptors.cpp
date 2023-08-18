#include <vk_check.hpp>
#include <vk_descriptors.hpp>
#include <vulkan/vulkan_core.h>

void DescriptorAllocator::init(VkDevice vkDevice,
                               VkDescriptorPoolCreateFlags vkFlags) {
  _vkDevice = vkDevice;
  _vkDescriptorPoolCreateFlags = vkFlags;

  _vkCurrentDescriptorPool = createNewDescriptorPool();
}

void DescriptorAllocator::cleanup() {
  for (VkDescriptorPool const pool : _usedDescriptorPools) {
    vkDestroyDescriptorPool(_vkDevice, pool, nullptr);
  }

  for (VkDescriptorPool const pool : _availableDescriptorPools) {
    vkDestroyDescriptorPool(_vkDevice, pool, nullptr);
  }
}

bool DescriptorAllocator::allocate(
    VkDescriptorSetLayout const& vkDescriptorSetLayout,
    VkDescriptorSet* outDescriptorSet) {
  VkDescriptorSetAllocateInfo vkDescriptorSetAllocateInfo = {};
  vkDescriptorSetAllocateInfo.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  vkDescriptorSetAllocateInfo.pNext = nullptr;

  vkDescriptorSetAllocateInfo.descriptorPool = _vkCurrentDescriptorPool;
  vkDescriptorSetAllocateInfo.descriptorSetCount = 1;
  vkDescriptorSetAllocateInfo.pSetLayouts = &vkDescriptorSetLayout;

  VkResult vkResult = vkAllocateDescriptorSets(
      _vkDevice, &vkDescriptorSetAllocateInfo, outDescriptorSet);

  if (vkResult == VK_ERROR_OUT_OF_POOL_MEMORY ||
      vkResult == VK_ERROR_FRAGMENTED_POOL) {
    // Retire current descriptor pool and retry with a new one
    _usedDescriptorPools.push_back(_vkCurrentDescriptorPool);
    _vkCurrentDescriptorPool = getAvailableDescriptorPool();

    vkResult = vkAllocateDescriptorSets(_vkDevice, &vkDescriptorSetAllocateInfo,
                                        outDescriptorSet);
  }

  return vkResult == VK_SUCCESS;
}

void DescriptorAllocator::resetPools() {
  vkResetDescriptorPool(_vkDevice, _vkCurrentDescriptorPool, 0);

  for (VkDescriptorPool const pool : _usedDescriptorPools) {
    vkResetDescriptorPool(_vkDevice, pool, 0);
    _availableDescriptorPools.push_back(pool);
  }

  _usedDescriptorPools.clear();
}

VkDescriptorPool DescriptorAllocator::getAvailableDescriptorPool() {
  if (_availableDescriptorPools.size()) {
    VkDescriptorPool availableDescriptorPool = _availableDescriptorPools.back();
    _availableDescriptorPools.pop_back();
    return availableDescriptorPool;
  }

  return createNewDescriptorPool();
}

VkDescriptorPool DescriptorAllocator::createNewDescriptorPool() {
  VkDescriptorPoolCreateInfo vkDescriptorPoolCreateInfo = {};
  vkDescriptorPoolCreateInfo.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  vkDescriptorPoolCreateInfo.pNext = nullptr;

  vkDescriptorPoolCreateInfo.flags = _vkDescriptorPoolCreateFlags;
  vkDescriptorPoolCreateInfo.maxSets = maxSets;

  constexpr std::size_t numDescriptorPoolSizes =
      sizeof(descriptorPoolSizes) / sizeof(descriptorPoolSizes[0]);

  std::array<VkDescriptorPoolSize, numDescriptorPoolSizes> newPoolSizes;

  for (std::size_t i = 0; i < numDescriptorPoolSizes; ++i) {
    newPoolSizes[i].type = descriptorPoolSizes[i].type;
    newPoolSizes[i].descriptorCount =
        _descriptorPoolSizeFactor * descriptorPoolSizes[i].descriptorCount;
  }

  vkDescriptorPoolCreateInfo.poolSizeCount = newPoolSizes.size();
  vkDescriptorPoolCreateInfo.pPoolSizes = newPoolSizes.data();

  VkDescriptorPool newDescriptorPool;

  VK_CHECK(vkCreateDescriptorPool(_vkDevice, &vkDescriptorPoolCreateInfo,
                                  nullptr, &newDescriptorPool));

  ++_descriptorPoolSizeFactor;

  return newDescriptorPool;
}
