#pragma once

#include <future>
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
void invokeTask(std::packaged_task<R(Arg)>& task, void const* argP) {
  task(*reinterpret_cast<Arg const*>(argP));
}

template <typename R>
void invokeTask(std::packaged_task<R(void)>& task, void const* argP) {
  task();
}

} /*namespace obsidian::core */
