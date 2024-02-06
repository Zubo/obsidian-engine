#include <obsidian/task/task.hpp>
#include <obsidian/task/task_executor.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
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

  std::vector<std::future<void>> futures;

  // act
  for (std::size_t i = 0; i < numberOfTasks; ++i) {
    futures.push_back(executor.enqueue(taskType, [&cnt]() {
      for (std::size_t j = 0; j < deltaPerTask; ++j) {
        ++cnt;
      }
    }));
  }

  // assert
  for (auto& f : futures) {
    f.wait();
  }

  ASSERT_EQ(cnt, deltaPerTask * numberOfTasks);
}
