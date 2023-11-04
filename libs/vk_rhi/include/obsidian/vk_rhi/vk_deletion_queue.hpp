#pragma once

#include <deque>
#include <functional>
#include <mutex>

namespace obsidian::vk_rhi {

class DeletionQueue {
public:
  template <typename TFunc> void pushFunction(TFunc&& f) {
    std::scoped_lock l{m};

    deletionFuncs.emplace_back(std::forward<TFunc>(f));
  }

  void flush() {
    std::scoped_lock l{m};

    for (auto deletionFuncIter = deletionFuncs.crbegin();
         deletionFuncIter != deletionFuncs.crend(); ++deletionFuncIter)
      (*deletionFuncIter)();

    deletionFuncs.clear();
  }

private:
  std::deque<std::function<void()>> deletionFuncs;
  std::mutex m;
};

} /*namespace obsidian::vk_rhi*/
