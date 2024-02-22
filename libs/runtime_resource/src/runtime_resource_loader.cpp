#include <obsidian/core/logging.hpp>
#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/runtime_resource/runtime_resource.hpp>
#include <obsidian/runtime_resource/runtime_resource_loader.hpp>
#include <obsidian/task/task_executor.hpp>
#include <obsidian/task/task_type.hpp>

#include <algorithm>
#include <mutex>
#include <thread>

using namespace obsidian::runtime_resource;

RuntimeResourceLoader::~RuntimeResourceLoader() { joinLoaderThread(); }

void RuntimeResourceLoader::run(task::TaskExecutor& taskExecutor) {
  _running = true;
  _taskExecutor = &taskExecutor;
  _loaderThread = std::thread{[this]() { uploaderFunc(); }};
}

void RuntimeResourceLoader::cleanup() {
  joinLoaderThread();
  _taskExecutor = nullptr;
}

bool RuntimeResourceLoader::loadResource(RuntimeResource& runtimeResource) {
  std::unique_lock l{_queueMutex};

  bool const uploadAppended = loadResImpl(runtimeResource);

  l.unlock();

  if (uploadAppended) {
    _queueMutexCondVar.notify_one();
  }

  return uploadAppended;
}

bool RuntimeResourceLoader::loadResImpl(RuntimeResource& runtimeResource) {
  if (runtimeResource.getResourceState() != RuntimeResourceState::pendingLoad) {
    return false;
  }

  _assetLoadQueue.push_back(&runtimeResource);
  return true;
}

void RuntimeResourceLoader::uploaderFunc() {
  while (_running) {
    std::unique_lock l{_queueMutex};

    _queueMutexCondVar.wait(l, [this]() {
      return !_running || _assetLoadQueue.size() || _rhiUploadQueue.size();
    });

    if (!_running) {
      return;
    }

    static std::vector<RuntimeResource*> resources;

    resources.swap(_rhiUploadQueue);

    for (RuntimeResource* r : resources) {
      if (r->getResourceState() != RuntimeResourceState::assetLoaded) {
        _rhiUploadQueue.push_back(r);
        continue;
      }

      std::span<RuntimeResourceRef> const deps = r->fetchDependencies();
      std::vector<RuntimeResourceRef> depsVec{deps.begin(), deps.end()};

      bool const depsReady = std::all_of(
          depsVec.begin(), depsVec.end(), [](RuntimeResourceRef& r) {
            return r->getResourceState() == RuntimeResourceState::uploadedToRhi;
          });

      if (depsReady) {
        _taskExecutor->enqueue(
            task::TaskType::rhiUpload,
            [r, /*hold references so they don't get deallocated*/ depsV =
                    std::move(depsVec)] { r->performUploadToRHI(); });
      } else {
        _rhiUploadQueue.push_back(r);
      }
    }

    resources.clear();

    resources.swap(_assetLoadQueue);

    for (RuntimeResource* r : resources) {
      if (r->getResourceState() == RuntimeResourceState::pendingLoad) {
        _taskExecutor->enqueue(task::TaskType::general,
                               [r]() { r->performAssetLoad(); });
        _rhiUploadQueue.push_back(r);
      } else {
        _assetLoadQueue.push_back(r);
      }
    }

    resources.clear();
  }
}

void RuntimeResourceLoader::joinLoaderThread() {
  std::unique_lock l{_queueMutex};

  _running = false;

  l.unlock();

  _queueMutexCondVar.notify_one();

  if (_loaderThread.joinable()) {
    _loaderThread.join();
  }
}
