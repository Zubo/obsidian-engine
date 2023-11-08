#pragma once

#include <cstddef>
#include <vector>

namespace obsidian::core::utils {

std::vector<unsigned char> reduceTextureSize(unsigned char const* srcData,
                                             std::size_t pixelSize,
                                             std::size_t w, std::size_t h,
                                             std::size_t reductionFactor,
                                             bool isLinear);

} // namespace obsidian::core::utils
