#include <obsidian/task/task.hpp>

using namespace obsidian::task;

std::atomic<TaskId> TaskBase::nextTaskId = 1;

TaskBase::TaskBase(TaskType type) : _taskId(nextTaskId++), _type{type} {}

void TaskBase::setArg(void const* argPtr) { _argPtr = argPtr; }

TaskId TaskBase::getId() const { return _taskId; };

TaskType TaskBase::getType() const { return _type; }
