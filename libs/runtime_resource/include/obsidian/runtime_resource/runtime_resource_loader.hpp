#pragma once

#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

namespace obsidian::task {

class TaskExecutor;

} /*namespace obsidian::task*/

namespace obsidian::runtime_resource {

class RuntimeResource;

class RuntimeResourceLoader {
public:
  RuntimeResourceLoader() = default;
  RuntimeResourceLoader(RuntimeResourceLoader const& other) = delete;

  void run(task::TaskExecutor& taskExecutor);
  void cleanup();
  ~RuntimeResourceLoader();

  RuntimeResourceLoader& operator=(RuntimeResourceLoader const& other) = delete;

  bool loadResource(RuntimeResource& runtimeResource);

private:
  bool loadResImpl(RuntimeResource& runtimeResource);
  void uploaderFunc();
  void joinLoaderThread();

  task::TaskExecutor* _taskExecutor;
  std::mutex _queueMutex;
  std::condition_variable _queueMutexCondVar;
  std::thread _loaderThread;
  std::vector<RuntimeResource*> _assetLoadQueue;
  std::vector<RuntimeResource*> _rhiUploadQueue;
  bool _running = false;
};

} /*namespace obsidian::runtime_resource*/
