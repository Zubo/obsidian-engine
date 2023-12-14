#include <obsidian/core/utils/texture_utils.hpp>
#include <obsidian/core/utils/utils.hpp>

#include <cassert>
#include <cmath>
#include <vector>

namespace obsidian::core::utils {

std::vector<unsigned char> reduceTextureSize(unsigned char const* srcData,
                                             std::size_t channelCnt,
                                             std::size_t w, std::size_t h,
                                             std::size_t reductionFactor,
                                             std::size_t nonLinearChannelCnt) {
  assert(isPowerOfTwo(w) && isPowerOfTwo(h));
  assert(nonLinearChannelCnt <= channelCnt);

  std::vector<unsigned char> result;

  auto const linearizeF = [](float v) {
    // Formula taken from https://en.wikipedia.org/wiki/SRGB
    return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
  };

  auto const delinearizeF = [](float v) {
    // Formula taken from https://en.wikipedia.org/wiki/SRGB
    return (v <= 0.04045f / 12.92f) ? v * 12.92f
                                    : std::pow(v, 1 / 2.4f) * 1.055f - 0.055f;
  };

  int const newW = w / reductionFactor;
  int const newH = h / reductionFactor;

  result.resize(newW * newH * channelCnt);

  for (std::size_t y = 0; y < newH; ++y) {
    for (std::size_t x = 0; x < newW; ++x) {
      std::vector<float> sumPix;
      sumPix.resize(channelCnt, 0.0f);

      for (std::size_t blockX = 0; blockX < reductionFactor; ++blockX) {
        for (std::size_t blockY = 0; blockY < reductionFactor; ++blockY) {
          unsigned char const* const srcPixData =
              srcData + channelCnt * (w * (reductionFactor * y + blockY) +
                                      reductionFactor * x + blockX);

          for (std::size_t i = 0; i < nonLinearChannelCnt; ++i) {
            float const linearized = linearizeF(srcPixData[i] / 255.0f);
            sumPix[i] += linearized;
          }

          for (std::size_t i = nonLinearChannelCnt; i < sumPix.size(); ++i) {
            sumPix[i] += srcPixData[i] / 255.0f;
          }
        }
      }

      unsigned char* const dstPixData =
          result.data() + channelCnt * ((y * newH) + x);
      for (std::size_t i = 0; i < nonLinearChannelCnt; ++i) {
        float const delinearized =
            delinearizeF(sumPix[i] / (reductionFactor * reductionFactor)) *
            255.0f;
        dstPixData[i] = static_cast<unsigned char>(delinearized);
      }

      for (std::size_t i = nonLinearChannelCnt; i < sumPix.size(); ++i) {
        dstPixData[i] = static_cast<unsigned char>(
            255.0f * sumPix[i] / (reductionFactor * reductionFactor));
      }
    }
  }

  return result;
}

} // namespace obsidian::core::utils
