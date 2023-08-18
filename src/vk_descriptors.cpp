#include <vk_check.hpp>
#include <vk_descriptors.hpp>

#include <crc32.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_handles.hpp>

#include <algorithm>

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

bool DescriptorLayoutCache::DescriptorLayoutInfo::operator==(
    DescriptorLayoutInfo const& other) const {
  if (other.flags != flags)
    return false;

  if (other.descriptorLayoutBindings.size() != descriptorLayoutBindings.size())
    return false;

  for (std::size_t i; i < descriptorLayoutBindings.size(); ++i) {
    if (other.descriptorLayoutBindings[i].binding !=
        descriptorLayoutBindings[i].binding)
      return false;
    if (other.descriptorLayoutBindings[i].descriptorCount !=
        descriptorLayoutBindings[i].descriptorCount)
      return false;
    if (other.descriptorLayoutBindings[i].descriptorType !=
        descriptorLayoutBindings[i].descriptorType)
      return false;
    if (other.descriptorLayoutBindings[i].stageFlags !=
        descriptorLayoutBindings[i].stageFlags)
      return false;
    if (other.descriptorLayoutBindings[i].pImmutableSamplers !=
        descriptorLayoutBindings[i].pImmutableSamplers)
      return false;
  }

  return true;
}

void DescriptorLayoutCache::init(VkDevice vkDevice) { _vkDevice = vkDevice; }

void DescriptorLayoutCache::cleanup() {
  for (auto const& layoutMapEntry : _descriptorSetLayoutMap) {
    vkDestroyDescriptorSetLayout(_vkDevice, layoutMapEntry.second, nullptr);
  }
}

VkDescriptorSetLayout DescriptorLayoutCache::getLayout(
    VkDescriptorSetLayoutCreateInfo const& descriptorSetLayoutCreateInfo) {

  DescriptorLayoutInfo info = {};
  info.flags = descriptorSetLayoutCreateInfo.flags;

  for (std::size_t i = 0; i < descriptorSetLayoutCreateInfo.bindingCount; ++i) {
    info.descriptorLayoutBindings.push_back(
        descriptorSetLayoutCreateInfo.pBindings[i]);
  }

  std::sort(info.descriptorLayoutBindings.begin(),
            info.descriptorLayoutBindings.end(),
            [](auto const& first, auto const& second) {
              return first.binding < second.binding;
            });

  auto const cachedLayoutIter = _descriptorSetLayoutMap.find(info);

  if (cachedLayoutIter != _descriptorSetLayoutMap.cend()) {
    return cachedLayoutIter->second;
  }

  VkDescriptorSetLayoutCreateInfo vkDescriptorSetLayoutCreateInfo = {};
  vkDescriptorSetLayoutCreateInfo.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  vkDescriptorSetLayoutCreateInfo.pNext = nullptr;

  vkDescriptorSetLayoutCreateInfo.flags = info.flags;
  vkDescriptorSetLayoutCreateInfo.bindingCount =
      info.descriptorLayoutBindings.size();
  vkDescriptorSetLayoutCreateInfo.pBindings =
      info.descriptorLayoutBindings.data();

  VkDescriptorSetLayout& newLayout = _descriptorSetLayoutMap[info];

  VK_CHECK(vkCreateDescriptorSetLayout(
      _vkDevice, &vkDescriptorSetLayoutCreateInfo, nullptr, &newLayout));

  return newLayout;
}

std::size_t DescriptorLayoutCache::DescriptorLayoutInfoHash::operator()(
    DescriptorLayoutInfo const& descriptorLayoutInfo) const {
  // TODO: unit test this

  CRC32 crc32;
  crc32.add(static_cast<void const*>(&descriptorLayoutInfo),
            sizeof(DescriptorLayoutInfo));

  for (VkDescriptorSetLayoutBinding const& layoutBinding :
       descriptorLayoutInfo.descriptorLayoutBindings) {
    crc32.add(&layoutBinding, sizeof(layoutBinding));
  }

  std::size_t result;
  crc32.getHash(reinterpret_cast<unsigned char*>(&result));

  return result;
}
