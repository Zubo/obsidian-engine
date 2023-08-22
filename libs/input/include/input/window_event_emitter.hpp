#pragma once

#include <functional>
#include <vector>
namespace obsidian::input {

class WindowEventEmitter {
public:
  using WindowResizedHandler = std::function<void(std::size_t, std::size_t)>;
  using FocusChangedHandler = std::function<void(bool)>;

  WindowEventEmitter() = default;
  WindowEventEmitter(WindowEventEmitter const& other) = delete;

  void subscribeToWindowResizedEvent(WindowResizedHandler windowResizedHandler);
  void subscribeToFocusChangedEvent(FocusChangedHandler focusChangedHandler);

  void fireWindowResizedEvent(std::size_t newWidth, std::size_t newHeight);
  void fireFocusChangedEvent(bool hasFocus);

  void cleanup();

private:
  std::vector<WindowResizedHandler> _windowResizedHandlers;
  std::vector<FocusChangedHandler> _focusChangedHandlers;
};

} // namespace obsidian::input
