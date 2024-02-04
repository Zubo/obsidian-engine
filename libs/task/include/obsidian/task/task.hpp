#pragma once

#include <obsidian/core/logging.hpp>
#include <obsidian/core/utils/functions.hpp>
#include <obsidian/task/task_type.hpp>

#include <atomic>
#include <cassert>
#include <functional>
#include <future>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace obsidian::task {

using TaskId = std::size_t;

template <typename F> class Task;

class TaskBase {
public:
  TaskBase(TaskType type);
  virtual ~TaskBase() = default;

  virtual void execute() = 0;
  virtual void setArg(std::shared_ptr<void const> const& argPtr);

  TaskId getId() const;

  TaskType getType() const;

  bool isDone() const;

protected:
  std::shared_ptr<void const> _argPtr = nullptr;
  std::atomic<bool> _done = false;

private:
  static std::atomic<TaskId> nextTaskId;

  std::atomic<TaskId> _taskId;
  std::atomic<TaskType> _type;
};

template <typename F> class Task : public TaskBase {
  using PackagedTaskType = decltype(std::packaged_task(std::declval<F>()));

public:
  Task(TaskType type, F&& func)
      : TaskBase(type), _packagedTask{std::forward<F>(func)} {}

  auto getFuture() { return _packagedTask.get_future(); }

  void execute() override {
    if (_done) {
      OBS_LOG_ERR("Trying to execute a task that is already done.");
      return;
    }

    core::invokeTask(_packagedTask, _argPtr.get());

    _done = true;
  }

private:
  PackagedTaskType _packagedTask;
};

} /*namespace obsidian::task*/
