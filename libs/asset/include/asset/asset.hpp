#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace obsidian::asset {

struct Asset {
  using SizeType = std::uint64_t;

  char type[4];
  std::uint32_t version;
  std::string json;
  std::vector<char> binaryBlob;
};

} /*namespace obsidian::asset*/
