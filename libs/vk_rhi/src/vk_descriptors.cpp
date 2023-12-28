#include <obsidian/vk_rhi/vk_check.hpp>
#include <obsidian/vk_rhi/vk_descriptors.hpp>

#include <crc32.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>

using namespace obsidian::vk_rhi;

void DescriptorAllocator::init(VkDevice vkDevice,
                               VkDescriptorPoolCreateFlags vkFlags) {
  _vkDevice = vkDevice;
  _vkDescriptorPoolCreateFlags = vkFlags;

  _vkCurrentDescriptorPool = createNewDescriptorPool();
}

void DescriptorAllocator::cleanup() {
  vkDestroyDescriptorPool(_vkDevice, _vkCurrentDescriptorPool, nullptr);

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

  constexpr std::size_t numDescriptorPoolSizes = std::size(descriptorPoolSizes);

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
  if (other.descriptorLayoutCreateFlags != descriptorLayoutCreateFlags)
    return false;

  if (other.descriptorLayoutBindings.size() != descriptorLayoutBindings.size())
    return false;

  for (std::size_t i = 0; i < descriptorLayoutBindings.size(); ++i) {
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
    if (other.descriptorBindingFlags != descriptorBindingFlags) {
      return false;
    }
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
    VkDescriptorSetLayoutCreateInfo const& vkDescriptorSetLayoutCreateInfo) {

  DescriptorLayoutInfo info = {};
  info.descriptorLayoutCreateFlags = vkDescriptorSetLayoutCreateInfo.flags;

  for (std::size_t i = 0; i < vkDescriptorSetLayoutCreateInfo.bindingCount;
       ++i) {
    info.descriptorLayoutBindings.push_back(
        vkDescriptorSetLayoutCreateInfo.pBindings[i]);
  }

  if (vkDescriptorSetLayoutCreateInfo.pNext) {
    VkDescriptorSetLayoutBindingFlagsCreateInfo const* const
        bindingFlagsCreateInfo = reinterpret_cast<
            VkDescriptorSetLayoutBindingFlagsCreateInfo const*>(
            vkDescriptorSetLayoutCreateInfo.pNext);

    if (bindingFlagsCreateInfo->sType ==
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO) {
      for (std::size_t i = 0; i < bindingFlagsCreateInfo->bindingCount; ++i) {
        info.descriptorBindingFlags.push_back(
            bindingFlagsCreateInfo->pBindingFlags[i]);
      }
    }
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

  for (VkDescriptorBindingFlags flags :
       descriptorLayoutInfo.descriptorBindingFlags) {
    crc32.add(&flags, sizeof(flags));
  }

  std::size_t result;
  crc32.getHash(reinterpret_cast<unsigned char*>(&result));

  return result;
}

DescriptorBuilder DescriptorBuilder::begin(VkDevice vkDevice,
                                           DescriptorAllocator& allocator,
                                           DescriptorLayoutCache& layoutCache) {

  return DescriptorBuilder{vkDevice, allocator, layoutCache};
}

DescriptorBuilder&
DescriptorBuilder::setFlags(VkDescriptorSetLayoutCreateFlags flags) {
  _flags = flags;

  return *this;
}

DescriptorBuilder& DescriptorBuilder::bindBuffer(
    uint32_t binding, VkDescriptorBufferInfo const& bufferInfo,
    VkDescriptorType descriptorType, VkShaderStageFlags stageFlags,
    const VkSampler* pImmutableSamplers, bool partiallyBound) {

  VkDescriptorSetLayoutBinding& vkDescriptorSetLayoutBinding =
      _bindings.emplace_back();
  vkDescriptorSetLayoutBinding.binding = binding;
  vkDescriptorSetLayoutBinding.descriptorType = descriptorType;
  vkDescriptorSetLayoutBinding.descriptorCount = 1;
  vkDescriptorSetLayoutBinding.stageFlags = stageFlags;
  vkDescriptorSetLayoutBinding.pImmutableSamplers = pImmutableSamplers;

  if (partiallyBound) {
    partiallyBoundBinding(binding);
  }

  VkWriteDescriptorSet& vkWriteDescriptorSet = _writes.emplace_back();
  vkWriteDescriptorSet = {};
  vkWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  vkWriteDescriptorSet.pNext = nullptr;
  vkWriteDescriptorSet.dstBinding = binding;
  vkWriteDescriptorSet.dstArrayElement = 0;
  vkWriteDescriptorSet.descriptorCount = 1;
  vkWriteDescriptorSet.descriptorType = descriptorType;
  vkWriteDescriptorSet.pBufferInfo = &bufferInfo;

  return *this;
}

DescriptorBuilder&
DescriptorBuilder::declareUnusedBuffer(uint32_t binding,
                                       VkDescriptorType descriptorType,
                                       VkShaderStageFlags stageFlags) {
  VkDescriptorSetLayoutBinding& vkDescriptorSetLayoutBinding =
      _bindings.emplace_back();

  vkDescriptorSetLayoutBinding.binding = binding;
  vkDescriptorSetLayoutBinding.descriptorType = descriptorType;
  vkDescriptorSetLayoutBinding.descriptorCount = 1;
  vkDescriptorSetLayoutBinding.stageFlags = stageFlags;

  partiallyBoundBinding(binding);

  return *this;
}

DescriptorBuilder& DescriptorBuilder::bindImage(
    uint32_t binding, VkDescriptorImageInfo const& imageInfo,
    VkDescriptorType descriptorType, VkShaderStageFlags stageFlags,
    const VkSampler* pImmutableSamplers, bool partiallyBound) {

  VkDescriptorSetLayoutBinding& vkDescriptorSetLayoutBinding =
      _bindings.emplace_back();
  vkDescriptorSetLayoutBinding = {};
  vkDescriptorSetLayoutBinding.binding = binding;
  vkDescriptorSetLayoutBinding.descriptorType = descriptorType;
  vkDescriptorSetLayoutBinding.descriptorCount = 1;
  vkDescriptorSetLayoutBinding.stageFlags = stageFlags;
  vkDescriptorSetLayoutBinding.pImmutableSamplers = pImmutableSamplers;

  if (partiallyBound) {
    partiallyBoundBinding(binding);
  }

  VkWriteDescriptorSet& vkWriteDescriptorSet = _writes.emplace_back();
  vkWriteDescriptorSet = {};
  vkWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  vkWriteDescriptorSet.pNext = nullptr;
  vkWriteDescriptorSet.dstBinding = binding;
  vkWriteDescriptorSet.dstArrayElement = 0;
  vkWriteDescriptorSet.descriptorCount = 1;
  vkWriteDescriptorSet.descriptorType = descriptorType;
  vkWriteDescriptorSet.pImageInfo = &imageInfo;

  return *this;
}

DescriptorBuilder& DescriptorBuilder::bindImages(
    uint32_t binding, std::vector<VkDescriptorImageInfo> const& imageInfos,
    VkDescriptorType descriptorType, VkShaderStageFlags stageFlags,
    const VkSampler* pImmutableSamplers) {

  VkDescriptorSetLayoutBinding& vkDescriptorSetLayoutBinding =
      _bindings.emplace_back();
  vkDescriptorSetLayoutBinding = {};
  vkDescriptorSetLayoutBinding.binding = binding;
  vkDescriptorSetLayoutBinding.descriptorType = descriptorType;
  vkDescriptorSetLayoutBinding.descriptorCount = imageInfos.size();
  vkDescriptorSetLayoutBinding.stageFlags = stageFlags;
  vkDescriptorSetLayoutBinding.pImmutableSamplers = pImmutableSamplers;

  VkWriteDescriptorSet& vkWriteDescriptorSet = _writes.emplace_back();
  vkWriteDescriptorSet = {};
  vkWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  vkWriteDescriptorSet.pNext = nullptr;
  vkWriteDescriptorSet.dstBinding = binding;
  vkWriteDescriptorSet.dstArrayElement = 0;
  vkWriteDescriptorSet.descriptorCount = imageInfos.size();
  vkWriteDescriptorSet.descriptorType = descriptorType;
  vkWriteDescriptorSet.pImageInfo = imageInfos.data();

  return *this;
}

DescriptorBuilder& DescriptorBuilder::declareUnusedImage(
    uint32_t binding, VkDescriptorType descriptorType,
    VkShaderStageFlags stageFlags, const VkSampler* pImmutableSamplers) {
  VkDescriptorSetLayoutBinding& vkDescriptorSetLayoutBinding =
      _bindings.emplace_back();
  vkDescriptorSetLayoutBinding = {};
  vkDescriptorSetLayoutBinding.binding = binding;
  vkDescriptorSetLayoutBinding.descriptorType = descriptorType;
  vkDescriptorSetLayoutBinding.descriptorCount = 1;
  vkDescriptorSetLayoutBinding.stageFlags = stageFlags;
  vkDescriptorSetLayoutBinding.pImmutableSamplers = pImmutableSamplers;

  partiallyBoundBinding(binding);

  return *this;
}

bool DescriptorBuilder::build(VkDescriptorSet& outVkDescriptorSet) {
  VkDescriptorSetLayout layout;
  return build(outVkDescriptorSet, layout);
}

bool DescriptorBuilder::build(VkDescriptorSet& outVkDescriptorSet,
                              VkDescriptorSetLayout& outLayout) {

  getLayout(outLayout);

  bool result = _allocator->allocate(outLayout, &outVkDescriptorSet);

  for (VkWriteDescriptorSet& writeDescr : _writes) {
    writeDescr.dstSet = outVkDescriptorSet;
  }

  vkUpdateDescriptorSets(_vkDevice, _writes.size(), _writes.data(), 0, nullptr);

  return result;
}

DescriptorBuilder&
DescriptorBuilder::getLayout(VkDescriptorSetLayout& outLayout) {
  VkDescriptorSetLayoutCreateInfo vkDescriptorSetLayoutCreateInfo = {};
  vkDescriptorSetLayoutCreateInfo.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

  VkDescriptorSetLayoutBindingFlagsCreateInfo flagsCreateInfo = {};

  if (_bindingCreateFlags.size()) {
    flagsCreateInfo.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    flagsCreateInfo.pNext = nullptr;
    flagsCreateInfo.bindingCount = _bindingCreateFlags.size();
    flagsCreateInfo.pBindingFlags = _bindingCreateFlags.data();

    vkDescriptorSetLayoutCreateInfo.pNext = &flagsCreateInfo;
  } else {
    vkDescriptorSetLayoutCreateInfo.pNext = nullptr;
  }

  vkDescriptorSetLayoutCreateInfo.flags = _flags;
  vkDescriptorSetLayoutCreateInfo.bindingCount = _bindings.size();
  vkDescriptorSetLayoutCreateInfo.pBindings = _bindings.data();

  outLayout = _layoutCache->getLayout(vkDescriptorSetLayoutCreateInfo);

  return *this;
}

DescriptorBuilder::DescriptorBuilder(VkDevice vkDevice,
                                     DescriptorAllocator& allocator,
                                     DescriptorLayoutCache& layoutCache)

    : _vkDevice{vkDevice}, _allocator{&allocator}, _layoutCache{&layoutCache} {}
void DescriptorBuilder::partiallyBoundBinding(uint32_t binding) {
  if (binding >= _bindingCreateFlags.size()) {
    _bindingCreateFlags.resize(binding + 1, 0);
  }

  _bindingCreateFlags[binding] |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
}
