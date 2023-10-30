#pragma once

#include <obsidian/task/task_type.hpp>

#include <cassert>
#include <functional>
#include <type_traits>
#include <utility>

namespace obsidian::task {

class Task {
public:
  Task(TaskType type);
  virtual ~Task() = default;

  virtual void* getReturnVal() = 0;

  TaskType type;
};

template <typename F, typename Enabled = void> class TaskImpl : Task {
  using ReturnValueType = decltype(std::declval(F()));

public:
  TaskImpl(TaskType type, F&& func) : Task(type) {}

  virtual void* getReturnVal() { return &_returnVal; }

private:
  std::function<void(void)> _func;
  ReturnValueType _returnVal;
};

template <typename F> constexpr bool returnsVoid() {
  return std::is_void_v<decltype(std::declval(F()))>;
}

template <typename F>
class TaskImpl<F, std::enable_if_t<returnsVoid<F>()>> : Task {
public:
  TaskImpl(TaskType type, F&& f) : Task(type), _func{std::forward<F>(f)} {}

  virtual void* getReturnVal() {
    assert(false && "Return value doesn't exist");
  }

private:
  std::function<void(void)> _func;
};

} /*namespace obsidian::task*/
