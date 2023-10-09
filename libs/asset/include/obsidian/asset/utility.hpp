#pragma once

#include <span>
#include <vector>

namespace obsidian::asset {

bool compress(std::span<const char> src, std::vector<char>& outDst);

} // namespace obsidian::asset
