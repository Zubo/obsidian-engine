#include <rhi/rhi.hpp>
#include <rhi_vk/vk_engine.hpp>

using namespace obsidian;

static rhi_vk::VulkanEngine _impl;

void rhi::RHI::init() { _impl.init(); }

void rhi::RHI::run() { _impl.run(); }

void rhi::RHI::cleanup() { _impl.cleanup(); }
