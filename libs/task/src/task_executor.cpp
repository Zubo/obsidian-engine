#include <obsidian/task/task.hpp>
#include <obsidian/task/task_executor.hpp>

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>

using namespace obsidian::task;

void TaskExecutor::initAndRun(std::vector<ThreadInitInfo> threadInit) {
  std::scoped_lock l{_taskQueueMutex};

  _shutdownComplete = false;
  _running = true;

  for (ThreadInitInfo const& initInfo : threadInit) {
    auto const iter = _taskQueues.try_emplace(initInfo.taskType);
    assert(iter.second);

    for (std::size_t i = 0; i < initInfo.threadCount; ++i) {
      _threads.emplace_back([this, initInfo]() {
        workerFunc(initInfo.taskType, initInfo.callOnInterval,
                   initInfo.intervalMilliseconds);
      });
    }
  }
}

TaskExecutor::~TaskExecutor() {
  if (_running) {
    shutdown();
  }
}

void TaskExecutor::workerFunc(
    TaskType taskType, ThreadInitInfo::CallOnIntervalFunction intervalFunc,
    std::size_t intervalMilliseconds) {
  using namespace std::chrono;
  using Clock = high_resolution_clock;
  Clock::time_point lastTime = Clock::now();
  duration<std::size_t, std::milli> const interval(
      intervalFunc ? intervalMilliseconds : 10'000'000'000'000);

  assert(!intervalFunc || intervalMilliseconds != 0);

  std::unique_lock lock{_taskQueueMutex};

  TaskQueue& taskQueue = _taskQueues.at(taskType);

  lock.unlock();
  bool notifyWait;

  while (true) {
    notifyWait = false;

    lock.lock();

    taskQueue.taskQueueCondVar.wait_until(
        lock, lastTime + interval, [this, &taskQueue]() {
          return !_running || taskQueue.tasks.size() > 0;
        });

    if (!_running) {
      return;
    }

    if (intervalFunc && Clock::now() > lastTime + interval) {
      intervalFunc();
      lastTime = Clock::now();
    }

    if (!taskQueue.tasks.empty()) {
      std::unique_ptr<TaskBase> task = std::move(taskQueue.tasks.back());
      taskQueue.tasks.pop_back();

      ++taskQueue.tasksInProgress;

      lock.unlock();
      taskQueue.taskQueueCondVar.notify_one();

      task->execute();

      lock.lock();

      --taskQueue.tasksInProgress;
    }

    notifyWait = !getPendingAndUncompletedTasksCount();

    lock.unlock();

    if (notifyWait) {
      _waitIdleCondVar.notify_all();
    }
  }
}

void TaskExecutor::waitIdle() const {
  std::unique_lock l{_taskQueueMutex};

  _waitIdleCondVar.wait(
      l, [this]() { return !getPendingAndUncompletedTasksCount(); });
}

void TaskExecutor::shutdown() {
  _running = false;

  for (auto& queuePair : _taskQueues) {
    queuePair.second.taskQueueCondVar.notify_all();
  }

  for (std::thread& t : _threads) {
    t.join();
  }

  _waitIdleCondVar.notify_all();

  _taskQueues.clear();
  _threads.clear();
  _shutdownComplete = true;
}

bool TaskExecutor::shutdownComplete() const { return _shutdownComplete; }

std::size_t TaskExecutor::getPendingAndUncompletedTasksCount() const {
  std::size_t cnt = 0;

  for (auto const& taskQueueEntry : _taskQueues) {
    cnt += (taskQueueEntry.second.tasksInProgress +
            taskQueueEntry.second.tasks.size());
  }

  return cnt;
}
