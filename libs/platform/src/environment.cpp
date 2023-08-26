#include <obsidian/platform/environment.hpp>

#ifdef __linux__
#include <sys/param.h>
#include <unistd.h>
#endif

#include <cstddef>

namespace fs = std::filesystem;

namespace obsidian::platform {

fs::path getExecutableFilePath() {
  char buff[256];
  constexpr std::size_t len = sizeof(buff);

#ifdef __linux__
  int bytes = MIN(readlink("/proc/self/exe", buff, len), len - 1);

  if (bytes > 0) {
    buff[bytes] = '\0';
  }

  return buff;
#else
  static_assert(false && "Platform not supported.");
#endif
}

fs::path getExecutableDirectoryPath() {
  fs::path const exePath = getExecutableFilePath();
  return exePath.parent_path();
}

} /*namespace obsidian::platform*/
