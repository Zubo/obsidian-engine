#include <obsidian/core/utils/texture_utils.hpp>
#include <obsidian/core/utils/utils.hpp>

#include <cassert>
#include <cmath>
#include <vector>

namespace obsidian::core::utils {

std::vector<unsigned char> reduceTextureSize(unsigned char const* srcData,
                                             std::size_t pixelSize,
                                             std::size_t w, std::size_t h,
                                             std::size_t reductionFactor,
                                             bool isLinear) {
  assert(isPowerOfTwo(w) && isPowerOfTwo(h));

  std::vector<unsigned char> result;

  auto const linearizeF = isLinear ? [](float v) { return v; } : [](float v) {
    // Formula taken from https://en.wikipedia.org/wiki/SRGB
    return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
  };

  auto const delinearizeF = isLinear ? [](float v) { return v; } : [](float v) {
    // Formula taken from https://en.wikipedia.org/wiki/SRGB
    return (v <= 0.04045f / 12.92f) ? v * 12.92f
                                    : std::pow(v, 1 / 2.4f) * 1.055f - 0.055f;
  };

  int const newW = w / reductionFactor;
  int const newH = h / reductionFactor;

  result.resize(newW * newH * pixelSize);

  for (std::size_t y = 0; y < newH; ++y) {
    for (std::size_t x = 0; x < newW; ++x) {
      std::vector<float> sumPix;
      sumPix.resize(pixelSize, 0.0f);

      for (std::size_t blockX = 0; blockX < reductionFactor; ++blockX) {
        for (std::size_t blockY = 0; blockY < reductionFactor; ++blockY) {
          unsigned char const* const srcPixData =
              srcData + pixelSize * (w * (reductionFactor * y + blockY) +
                                     reductionFactor * x + blockX);

          for (std::size_t i = 0; i < sumPix.size(); ++i) {
            float const linearized = linearizeF(srcPixData[i] / 255.0f);
            sumPix[i] += linearized;
          }
        }
      }

      unsigned char* const dstPixData =
          result.data() + pixelSize * ((y * newH) + x);
      for (std::size_t i = 0; i < sumPix.size(); ++i) {
        float const delinearized =
            delinearizeF(sumPix[i] / (reductionFactor * reductionFactor)) *
            255.0f;
        dstPixData[i] = static_cast<unsigned char>(delinearized);
      }
    }
  }

  return result;
}

} // namespace obsidian::core::utils
