#pragma once

#include <cstddef>
#include <vector>

namespace obsidian::core::utils {

std::vector<unsigned char> reduceTextureSize(unsigned char const* srcData,
                                             std::size_t channelCnt,
                                             std::size_t w, std::size_t h,
                                             std::size_t reductionFactor,
                                             std::size_t nonLinearChannelCnt);

} // namespace obsidian::core::utils
