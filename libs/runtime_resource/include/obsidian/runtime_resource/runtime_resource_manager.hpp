#pragma once

#include <obsidian/project/project.hpp>
#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/runtime_resource/runtime_resource.hpp>
#include <obsidian/runtime_resource/runtime_resource_loader.hpp>

#include <filesystem>
#include <unordered_map>

namespace obsidian::rhi {

class RHI;

} // namespace obsidian::rhi

namespace obsidian::project {

class Project;

} /*namespace obsidian::project */

namespace task {

class TaskExecutor;

} /*namespace task */

namespace obsidian::runtime_resource {

class RuntimeResourceManager {
public:
  RuntimeResourceManager() = default;

  RuntimeResourceManager(RuntimeResourceManager const& other) = delete;

  void init(rhi::RHI& rhi, project::Project& project,
            task::TaskExecutor& taskExecutor);

  void uploadInitRHIResources();

  void cleanup();

  RuntimeResource& getResource(std::filesystem::path const& path);

  project::Project const& getProject() const;

  RuntimeResourceManager&
  operator=(RuntimeResourceManager const& other) = delete;

private:
  rhi::RHI* _rhi = nullptr;
  project::Project* _project = nullptr;
  task::TaskExecutor* _taskExecutor = nullptr;
  std::unordered_map<std::filesystem::path, RuntimeResource> _runtimeResources;
  RuntimeResourceLoader _resourceLoader;
};

} /*namespace obsidian::runtime_resource*/
