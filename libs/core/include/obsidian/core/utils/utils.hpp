#pragma once

#include <type_traits>

namespace obsidian::core {

template <typename T> bool isPowerOfTwo(T value) {
  static_assert(std::is_integral_v<T>,
                "This function only works for integral types.");

  return (value && (value & (value - 1)) == 0);
}

} /*namespace obsidian::core */
