#pragma once

#include <obsidian/core/utils/functions.hpp>
#include <obsidian/task/task_type.hpp>

#include <cassert>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace obsidian::task {

using TaskId = std::size_t;

class TaskBase {
public:
  TaskBase(TaskType type);
  virtual ~TaskBase() = default;

  virtual void* getReturnVal() = 0;
  virtual void execute(void* p) = 0;

  TaskId taskId;
  TaskType type;
  std::vector<std::unique_ptr<TaskBase>> followupTasks;
};

template <typename F, typename Enable = void> class Task : public TaskBase {
  using FuncType = decltype(std::function(std::declval<F>()));

public:
  Task(TaskType type, F&& func)
      : TaskBase(type), _func{std::forward<F>(func)} {}

  void* getReturnVal() override { return &_returnVal; }

  void execute(void* argP = nullptr) override {
    _returnVal = core::invokeFunc(_func, argP);
  }

private:
  FuncType _func;
  FuncType::result_type _returnVal;
};

template <typename F>
class Task<F, typename std::enable_if_t<std::is_void_v<core::ResultOf<F>>>>
    : public TaskBase {
  using FuncType = decltype(std::function(std::declval<F>()));

public:
  Task(TaskType type, F&& func)
      : TaskBase(type), _func{std::forward<F>(func)} {}

  void* getReturnVal() override { return nullptr; }

  void execute(void* argP = nullptr) override { core::invokeFunc(_func, argP); }

private:
  FuncType _func;
};

} /*namespace obsidian::task*/
