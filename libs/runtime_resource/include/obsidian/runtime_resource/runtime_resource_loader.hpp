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
  void init(task::TaskExecutor& taskExecutor);
  ~RuntimeResourceLoader();

  void uploadResource(RuntimeResource& runtimeResource);

private:
  bool uploadResImpl(RuntimeResource& runtimeResource);
  void uploaderFunc();

  task::TaskExecutor* _taskExecutor;
  std::mutex _queueMutex;
  std::condition_variable _queueMutexCondVar;
  std::thread _loaderThread;
  std::vector<RuntimeResource*> _assetLoadQueue;
  std::vector<RuntimeResource*> _rhiUploadQueue;
  bool _running = false;
  std::unordered_set<std::filesystem::path> _uploadedResources;
};

} /*namespace obsidian::runtime_resource*/
