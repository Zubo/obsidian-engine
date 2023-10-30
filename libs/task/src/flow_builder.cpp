#include <obsidian/task/flow_builder.hpp>

using namespace obsidian::task;

FlowBuilder FlowBuilder::begin(TaskExecutor& executor) {
  return FlowBuilder(executor);
}

FlowBuilder::FlowBuilder(TaskExecutor& executor) : _executor{executor} {}
