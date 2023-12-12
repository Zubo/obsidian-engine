#include <obsidian/task/task.hpp>
#include <obsidian/task/task_type.hpp>

#include <gtest/gtest.h>
#include <type_traits>

using namespace obsidian::task;

TEST(task, task_execute_no_args) {
  // arrange
  int modify = 0;

  Task t{TaskType::general, [&modify]() { modify = 1; }};

  // act
  t.execute();

  // assert
  EXPECT_EQ(modify, 1);
}

TEST(task, task_execute_with_args) {
  // arrange
  constexpr int startVal = -1;
  int result = startVal;
  std::shared_ptr<int> delta = std::make_shared<int>(2);

  Task t{TaskType::general, [&result](int delta) { result += delta; }};
  t.setArg(delta);

  // act
  t.execute();

  // assert
  EXPECT_EQ(result, startVal + *delta);
}

TEST(task, task_execute_ret_val) {
  // arrange
  constexpr const int val = 7234;
  Task t{TaskType::general, []() { return val; }};

  // act
  t.execute();

  // assert
  void const* returnVal = t.getReturn();
  EXPECT_NE(returnVal, nullptr);
  EXPECT_EQ(val, *reinterpret_cast<int const*>(returnVal));
}
