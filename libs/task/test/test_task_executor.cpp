#include <obsidian/task/task.hpp>
#include <obsidian/task/task_executor.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace obsidian::task;

TEST(task, task_executor_basic_execution) {
  // arrange
  constexpr TaskType taskType = TaskType::general;

  TaskExecutor executor;
  executor.initAndRun({{taskType, 8}});

  std::atomic<int> cnt = 0;
  constexpr int deltaPerTask = 1000000;
  constexpr int numberOfTasks = 100;

  std::vector<TaskBase const*> tasks;

  // act
  for (std::size_t i = 0; i < numberOfTasks; ++i) {
    tasks.push_back(&executor.enqueue(taskType, [&cnt]() {
      for (std::size_t j = 0; j < deltaPerTask; ++j) {
        ++cnt;
      }
    }));
  }

  // assert
  while (true) {
    if (std::all_of(tasks.cbegin(), tasks.cend(),
                    [](auto t) { return t->isDone(); })) {
      break;
    }
  }

  ASSERT_EQ(cnt, deltaPerTask * numberOfTasks);
}
