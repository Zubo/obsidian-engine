#include <rhi/rhi.hpp>
#include <vk_rhi/vk_engine.hpp>

using namespace obsidian;

static vk_rhi::VulkanEngine _impl;

void rhi::RHI::init() { _impl.init(); }

void rhi::RHI::run() { _impl.run(); }

void rhi::RHI::cleanup() { _impl.cleanup(); }
