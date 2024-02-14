#include <obsidian/platform/environment.hpp>

#ifdef __linux__
#include <sys/param.h>
#include <unistd.h>
#elif _WIN32
#include <Windows.h>
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

#else
  TCHAR buff[MAX_PATH];
  GetModuleFileName(NULL, buff, MAX_PATH);
#endif

  return buff;
}

fs::path getExecutableDirectoryPath() {
  fs::path const exePath = getExecutableFilePath();
  return exePath.parent_path();
}

} /*namespace obsidian::platform*/
