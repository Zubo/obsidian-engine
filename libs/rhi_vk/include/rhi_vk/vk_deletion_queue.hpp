#pragma once

#include <deque>
#include <functional>

namespace obsidian::rhi_vk {

class DeletionQueue {
public:
  template <typename TFunc> void pushFunction(TFunc&& f) {
    deletionFuncs.emplace_back(std::forward<TFunc>(f));
  }

  void flush() {
    for (auto deletionFuncIter = deletionFuncs.crbegin();
         deletionFuncIter != deletionFuncs.crend(); ++deletionFuncIter)
      (*deletionFuncIter)();

    deletionFuncs.clear();
  }

private:
  std::deque<std::function<void()>> deletionFuncs;
};

} /*namespace obsidian::rhi_vk*/
