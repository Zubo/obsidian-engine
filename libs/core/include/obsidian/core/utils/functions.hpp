#pragma once

#include <functional>
#include <type_traits>

namespace obsidian::core {

template <typename... Args> constexpr bool returnsVoid(void(Args...)) {
  return true;
}

template <typename R, typename... Args> constexpr bool returnsVoid(R(Args...)) {
  return false;
}

template <typename F>
using ResultOf = decltype(std::function(std::declval<F>()))::result_type;

template <typename R, typename Arg>
R invokeFunc(std::function<R(Arg)> const& func, void* argP) {
  return func(*reinterpret_cast<Arg*>(argP));
}

template <typename R>
R invokeFunc(std::function<R(void)> const& func, void* argP) {
  return func();
}

} /*namespace obsidian::core */
