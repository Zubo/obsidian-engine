#include <obsidian/core/utils/texture_utils.hpp>

#include <glm/glm.hpp>
#include <gtest/gtest.h>

#include <array>
#include <cstddef>

struct Pixel {
  unsigned char r, g, b, a;
};

TEST(texture_utils, reduce_linear_texture_size) {
  // arrange
  constexpr std::size_t w = 4, h = 4;

  constexpr std::array<Pixel, w * h> pixels{
      Pixel{0, 1, 0, 3},  Pixel{0, 1, 1, 3},  Pixel{0, 1, 2, 3},
      Pixel{0, 1, 3, 3},  Pixel{0, 1, 4, 3},  Pixel{0, 1, 5, 3},
      Pixel{0, 1, 6, 3},  Pixel{0, 1, 7, 3},  Pixel{0, 1, 8, 3},
      Pixel{0, 1, 9, 3},  Pixel{0, 1, 10, 3}, Pixel{0, 1, 11, 3},
      Pixel{0, 1, 12, 3}, Pixel{0, 1, 13, 3}, Pixel{0, 1, 14, 3},
      Pixel{0, 1, 15, 3},
  };
  constexpr std::size_t reductionFactor = 2;
  constexpr bool isLinear = true;

  // act
  std::vector<unsigned char> result = obsidian::core::utils::reduceTextureSize(
      reinterpret_cast<unsigned char const*>(pixels.data()), sizeof(Pixel), w,
      h, reductionFactor, isLinear);

  // assert
  constexpr unsigned char expectedR[] = {0, 0, 0, 0};
  constexpr unsigned char expectedG[] = {1, 1, 1, 1};
  constexpr unsigned char expectedB[] = {2, 4, 10, 12};
  constexpr unsigned char expectedA[] = {3, 3, 3, 3};

  std::size_t const newH = h / reductionFactor;
  std::size_t const newW = w / reductionFactor;

  Pixel const* resultData = reinterpret_cast<Pixel const*>(result.data());

  for (std::size_t y = 0; y < newH; ++y) {
    for (std::size_t x = 0; x < newW; ++x) {
      EXPECT_EQ(resultData[(y * newW) + x].r, expectedR[(y * newW) + x]);
      EXPECT_EQ(resultData[(y * newW) + x].g, expectedG[(y * newW) + x]);
      EXPECT_EQ(resultData[(y * newW) + x].b, expectedB[(y * newW) + x]);
      EXPECT_EQ(resultData[(y * newW) + x].a, expectedA[(y * newW) + x]);
    }
  }
}

TEST(texture_utils, reduce_non_linear_texture_size) {
  // arrange
  constexpr std::size_t w = 2, h = 2;

  constexpr std::array<Pixel, w * h> pixels{
      Pixel{0, 1, 2, 3}, Pixel{0, 1, 2, 3}, Pixel{0, 1, 2, 3},
      Pixel{0, 1, 2, 3}};

  constexpr std::size_t reductionFactor = 2;
  constexpr bool isLinear = false;

  // act
  std::vector<unsigned char> result = obsidian::core::utils::reduceTextureSize(
      reinterpret_cast<unsigned char const*>(pixels.data()), sizeof(Pixel), w,
      h, reductionFactor, isLinear);

  // assert
  Pixel expected = {0, 1, 2, 3};

  std::size_t const newH = h / reductionFactor;
  std::size_t const newW = w / reductionFactor;

  Pixel const* resultData = reinterpret_cast<Pixel const*>(result.data());

  for (std::size_t y = 0; y < newH; ++y) {
    for (std::size_t x = 0; x < newW; ++x) {
      EXPECT_EQ(resultData[(y * newW) + x].r, expected.r);
      EXPECT_EQ(resultData[(y * newW) + x].g, expected.g);
      EXPECT_EQ(resultData[(y * newW) + x].b, expected.b);
      EXPECT_EQ(resultData[(y * newW) + x].a, expected.a);
    }
  }
}
