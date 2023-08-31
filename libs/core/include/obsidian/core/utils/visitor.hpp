#pragma once

namespace obsidian::core {

template <typename... Args> struct visitor : public Args... {
  visitor(Args... args) : Args{args}... {}

  using Args::operator()...;
};

} /*namespace obsidian::core*/
