#pragma once

#include <nlohmann/json.hpp>

#include <array>
#include <cstring>

namespace obsidian::serialization {

template <typename VectorType,
          std::size_t arrSize = sizeof(VectorType) / sizeof(float)>
std::array<float, arrSize> vecToArray(VectorType const& v) {
  std::array<float, arrSize> outArray;
  std::memcpy(outArray.data(), &v.x, sizeof(VectorType));
  return outArray;
}

template <typename VectorType>
VectorType arrayToVector(nlohmann::json const& arrayJson, VectorType& result) {
  constexpr std::size_t numberOfElements = sizeof(VectorType) / sizeof(float);

  for (std::size_t i = 0; i < numberOfElements; ++i) {
    result[i] = arrayJson[i];
  }

  return result;
}

} /*namespace obsidian::serialization */
