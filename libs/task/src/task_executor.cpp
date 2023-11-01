#include <obsidian/task/task.hpp>
#include <obsidian/task/task_executor.hpp>

#include <condition_variable>
#include <memory>
#include <mutex>

using namespace obsidian::task;

TaskExecutor::TaskExecutor(std::vector<ThreadInitInfo> threadInit) {
  for (ThreadInitInfo const& initInfo : threadInit) {
    auto const iter = _taskQueues.try_emplace(initInfo.taskType);
    assert(iter.second);

    for (std::size_t i = 0; i < initInfo.threadCount; ++i) {
      _threads.emplace_back(
          [this, taskType = initInfo.taskType]() { workerFunc(taskType); });
    }
  }
}

void TaskExecutor::workerFunc(TaskType taskType) {
  TaskQueue& taskQueue = _taskQueues.at(taskType);

  std::unique_lock lock{taskQueueMutex, std::defer_lock};

  while (true) {
    lock.lock();

    taskQueue.taskQueueCondVar.wait(lock, [this, &taskQueue]() {
      return _shutdown || taskQueue.tasks.size() > 0;
    });

    if (_shutdown) {
      return;
    }

    std::unique_ptr<TaskBase> task = std::move(taskQueue.tasks.back());
    taskQueue.tasks.pop_back();

    lock.unlock();
    taskQueue.taskQueueCondVar.notify_one();

    task->execute();

    std::vector<std::unique_ptr<TaskBase>> followupTasks =
        task->transferFollowupTasks();

    std::vector<std::condition_variable*> condVarsToNotify;

    lock.lock();

    for (std::unique_ptr<TaskBase>& followupTask : followupTasks) {
      followupTask->setArg(task->getReturnVal());

      auto const taskQueue = _taskQueues.find(followupTask->getType());

      assert(taskQueue != _taskQueues.cend());

      taskQueue->second.tasks.push_back(std::move(followupTask));
      condVarsToNotify.push_back(&taskQueue->second.taskQueueCondVar);
    }

    lock.unlock();

    for (std::condition_variable* condVar : condVarsToNotify) {
      assert(condVar);
      condVar->notify_one();
    }
  }
}

void TaskExecutor::shutdown() {
  _shutdown = true;
  for (auto& queuePair : _taskQueues) {
    queuePair.second.taskQueueCondVar.notify_all();
  }

  for (std::thread& t : _threads) {
    t.join();
  }
}
