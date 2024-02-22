#pragma once

#include <obsidian/task/task.hpp>
#include <obsidian/task/task_type.hpp>

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace obsidian::task {

struct ThreadInitInfo {
  TaskType taskType;
  unsigned int threadCount;
};

struct TaskQueue {
  std::vector<std::unique_ptr<TaskBase>> tasks;
  std::condition_variable taskQueueCondVar;
  std::size_t tasksInProgress = 0;
};

class TaskExecutor {
public:
  ~TaskExecutor();

  void initAndRun(std::vector<ThreadInitInfo> threadInit);

  template <typename F> auto enqueue(TaskType type, F&& func) {
    auto const queue = _taskQueues.find(type);

    assert(queue != _taskQueues.cend());

    std::unique_lock l{_taskQueueMutex};

    using TaskType = Task<decltype(std::forward<F>(func))>;

    TaskBase& newTask = *queue->second.tasks.emplace_back(
        std::make_unique<TaskType>(type, std::forward<F>(func)));

    auto future = dynamic_cast<TaskType&>(newTask).getFuture();

    l.unlock();

    queue->second.taskQueueCondVar.notify_one();

    return future;
  }

  void workerFunc(TaskType taskType);

  void waitIdle() const;

  void shutdown();

  bool shutdownComplete() const;

  std::size_t getPendingAndUncompletedTasksCount() const;

private:
  std::map<TaskType, TaskQueue> _taskQueues;
  std::vector<std::thread> _threads;
  mutable std::mutex _taskQueueMutex;
  mutable std::condition_variable _waitIdleCondVar;
  std::atomic<bool> _running = false;
  std::atomic<bool> _shutdownComplete = false;
};

} /*namespace obsidian::task*/
