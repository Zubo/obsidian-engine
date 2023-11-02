#pragma once

#include <obsidian/task/task.hpp>
#include <obsidian/task/task_type.hpp>

#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace obsidian::task {

struct ThreadInitInfo {
  TaskType taskType;
  std::int8_t threadCount;
};

struct TaskQueue {
  std::vector<std::unique_ptr<TaskBase>> tasks;
  std::condition_variable taskQueueCondVar;
};

class TaskExecutor {
public:
  TaskExecutor(std::vector<ThreadInitInfo> threadInit);

  ~TaskExecutor();

  template <typename F> TaskBase& enqueue(TaskType type, F&& func) {
    auto const queue = _taskQueues.find(type);

    assert(queue != _taskQueues.cend());

    std::unique_lock l{taskQueueMutex};

    using TaskType = Task<decltype(std::forward<F>(func))>;

    TaskBase& newTask = *queue->second.tasks.emplace_back(
        std::make_unique<TaskType>(type, std::forward<F>(func)));

    l.unlock();

    queue->second.taskQueueCondVar.notify_one();

    return newTask;
  }

  void workerFunc(TaskType taskType);

  void shutdown();

private:
  std::unordered_map<TaskType, TaskQueue> _taskQueues;
  std::vector<std::unique_ptr<TaskBase>> _dequeuedTasks;
  std::vector<std::thread> _threads;
  std::mutex taskQueueMutex;
  bool _shutdown = false;
};

} /*namespace obsidian::task*/
