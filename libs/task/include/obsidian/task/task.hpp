#pragma once

#include <obsidian/core/utils/functions.hpp>
#include <obsidian/task/task_type.hpp>

#include <atomic>
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

  virtual std::shared_ptr<void const> getReturn() = 0;
  virtual void execute() = 0;
  virtual void setArg(std::shared_ptr<void const> const& argPtr);

  template <typename F> TaskBase& followedBy(TaskType type, F&& func) {
    return _followupTasks.emplace_back(type, std::forward(func));
  }

  template <typename... Fs> void followedBy(TaskType type, Fs&&... funcs) {
    (_followupTasks.emplace_back(std::forward<Fs>(funcs)), ...);
  }

  std::vector<std::unique_ptr<TaskBase>> transferFollowupTasks() {
    return std::move(_followupTasks);
  }

  TaskId getId() const;

  TaskType getType() const;

protected:
  std::shared_ptr<void const> _argPtr = nullptr;

private:
  static std::atomic<TaskId> nextTaskId;

  TaskId _taskId;
  TaskType _type;
  std::vector<std::unique_ptr<TaskBase>> _followupTasks;
};

template <typename F, typename Enable = void> class Task : public TaskBase {
  using FuncType = decltype(std::function(std::declval<F>()));

public:
  Task(TaskType type, F&& func)
      : TaskBase(type), _func{std::forward<F>(func)} {}

  std::shared_ptr<void const> getReturn() override { return _returnVal; }

  void execute() override {
    _returnVal = std::make_shared<typename FuncType::result_type const>(
        core::invokeFunc(_func, _argPtr.get()));
  }

private:
  FuncType _func;
  std::shared_ptr<typename FuncType::result_type const> _returnVal;
};

template <typename F>
class Task<F, typename std::enable_if_t<std::is_void_v<core::ResultOf<F>>>>
    : public TaskBase {
  using FuncType = decltype(std::function(std::declval<F>()));

public:
  Task(TaskType type, F&& func)
      : TaskBase(type), _func{std::forward<F>(func)} {}

  std::shared_ptr<void const> getReturn() override { return {}; }

  void execute() override { core::invokeFunc(_func, _argPtr.get()); }

private:
  FuncType _func;
};

} /*namespace obsidian::task*/
