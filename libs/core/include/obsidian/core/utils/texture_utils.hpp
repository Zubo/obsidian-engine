#pragma once

#include <cstddef>

namespace obsidian::core::utils {

void reduceTextureSize(unsigned char const* srcData, unsigned char* dstData,
                       std::size_t channelCnt, std::size_t w, std::size_t h,
                       std::size_t reductionFactor,
                       std::size_t nonLinearChannelCnt);

} // namespace obsidian::core::utils
