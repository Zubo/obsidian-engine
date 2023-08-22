#pragma once

#include <functional>
#include <vector>
namespace obsidian::input {

class WindowEventEmitter {
public:
  using WindowResizedHandler = std::function<void(std::size_t, std::size_t)>;

  WindowEventEmitter() = default;
  WindowEventEmitter(WindowEventEmitter const& other) = delete;

  void subscribeToWindowResizedEvent(WindowResizedHandler windowResizedHandler);

  void fireWindowResizedEvent(std::size_t newWidth, std::size_t newHeight);

  void cleanup();

private:
  std::vector<WindowResizedHandler> _windowResizedHandlers;
};

} // namespace obsidian::input
