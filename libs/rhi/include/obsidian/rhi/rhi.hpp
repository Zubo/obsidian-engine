#pragma once

#include <cstdint>
namespace obsidian::rhi {

using RHIResourceId = std::int64_t;
constexpr RHIResourceId rhiIdUninitialized = ~0;

} /*namespace obsidian::rhi*/
