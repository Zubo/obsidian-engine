#include <exception>
#include <filesystem>
#include <obsidian/core/logging.hpp>
#include <obsidian/editor/settings.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <ios>
#include <sstream>

namespace fs = std::filesystem;

namespace obsidian::editor {

constexpr const char* tempSettingsPath = "temp/editor-temp.json";
constexpr const char* lastOpenProjectJsonName = "lastOpenProject";

nlohmann::json& getJson() {
  static nlohmann::json tempSettings;

  if (tempSettings.empty()) {
    fs::path settingsPath{tempSettingsPath};

    if (fs::exists(settingsPath) && fs::is_regular_file(settingsPath)) {
      std::ifstream inpuFile{settingsPath};
      std::ostringstream sstr;
      sstr << inpuFile.rdbuf();
      try {
        tempSettings = nlohmann::json::parse(sstr.str());

      } catch (std::exception const& e) {
        OBS_LOG_ERR(e.what());
      }
    }
  }

  return tempSettings;
}

void saveJson() {
  fs::path settingsPath{tempSettingsPath};

  std::filesystem::create_directories(settingsPath.parent_path());
  std::ofstream fileStream(settingsPath,
                           std::ios_base::out | std::ios_base::trunc);
  try {
    fileStream << getJson().dump();
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
  }
}

fs::path getLastOpenProject() {
  try {
    nlohmann::json& json = getJson();

    if (!json.contains(lastOpenProjectJsonName)) {
      return {};
    }

    std::string proj = json[lastOpenProjectJsonName];
    return proj;
  } catch (std::exception const& e) {
    OBS_LOG_ERR(e.what());
    return {};
  }
}

void setLastOpenProject(fs::path const& path) {
  getJson()[lastOpenProjectJsonName] = path.string();
  saveJson();
}

} /*namespace obsidian::editor*/
