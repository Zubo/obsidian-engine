#include <obsidian/platform/environment.hpp>

#ifdef __linux__
#include <sys/param.h>
#include <unistd.h>
#elif _WIN32
#include <Windows.h>
#elif __APPLE__
#include <mach-o/dyld.h>
#include <sys/syslimits.h>
#endif

#include <cstddef>

namespace fs = std::filesystem;

namespace obsidian::platform {

fs::path getExecutableFilePath() {
#ifdef __linux__
  char buff[256];
  constexpr std::size_t len = sizeof(buff);

  int bytes = MIN(readlink("/proc/self/exe", buff, len), len - 1);

  if (bytes > 0) {
    buff[bytes] = '\0';
  }

#elif _WIN32
  TCHAR buff[MAX_PATH];
  GetModuleFileName(NULL, buff, MAX_PATH);
#elif __APPLE__
  char buff[PATH_MAX];
  std::uint32_t size = sizeof(buff);
  _NSGetExecutablePath(buff, &size);
#endif

  return buff;
}

fs::path getExecutableDirectoryPath() {
  fs::path const exePath = getExecutableFilePath();
  return exePath.parent_path();
}

} /*namespace obsidian::platform*/
