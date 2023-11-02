#pragma once

#include <obsidian/core/utils/functions.hpp>
#include <obsidian/task/task_type.hpp>

#include <atomic>
#include <cassert>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace obsidian::task {

using TaskId = std::size_t;

template <typename F> class Task;

class TaskBase {
public:
  TaskBase(TaskType type);
  virtual ~TaskBase() = default;

  virtual std::shared_ptr<void const> getReturn() const = 0;
  virtual void execute() = 0;
  virtual void setArg(std::shared_ptr<void const> const& argPtr);

  template <typename F> TaskBase& followedBy(TaskType type, F&& func) {
    using TaskType = Task<decltype(std::forward<F>(func))>;

    return *_followupTasks.emplace_back(
        std::make_unique<TaskType>(type, std::forward<F>(func)));
  }

  std::vector<std::unique_ptr<TaskBase>> transferFollowupTasks() {
    return std::move(_followupTasks);
  }

  TaskId getId() const;

  TaskType getType() const;

  bool getDone() const;

protected:
  std::shared_ptr<void const> _argPtr = nullptr;
  bool _done = false;

private:
  static std::atomic<TaskId> nextTaskId;

  TaskId _taskId;
  TaskType _type;
  std::vector<std::unique_ptr<TaskBase>> _followupTasks;
};

template <typename F> class Task : public TaskBase {
  using FuncType = decltype(std::function(std::declval<F>()));

public:
  Task(TaskType type, F&& func)
      : TaskBase(type), _func{std::forward<F>(func)} {}

  std::shared_ptr<void const> getReturn() const override { return _returnVal; }

  void execute() override {
    if constexpr (std::is_void_v<core::ResultOf<F>>) {
      core::invokeFunc(_func, _argPtr.get());
    } else {
      _returnVal = std::make_shared<typename FuncType::result_type const>(
          core::invokeFunc(_func, _argPtr.get()));
    }

    _done = true;
  }

private:
  FuncType _func;
  std::shared_ptr<core::ResultOf<F> const> _returnVal;
};

} /*namespace obsidian::task*/
