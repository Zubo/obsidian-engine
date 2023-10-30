#pragma once

#include <obsidian/task/flow.hpp>
#include <obsidian/task/task_executor.hpp>
#include <obsidian/task/task_type.hpp>

namespace obsidian::task {

class FlowBuilder {
public:
  static FlowBuilder begin(TaskExecutor& executor);

private:
  FlowBuilder(TaskExecutor& executor);

  TaskExecutor& _executor;
  Flow _flow;
};

} /*namespace obsidian::task */
