#pragma once

#include <cstddef>

namespace obsidian::task {

using FlowId = std::size_t;

struct Flow {
  FlowId id;
};

} /*namespace obsidian::task*/
