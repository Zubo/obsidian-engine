#include <input/window_event_emitter.hpp>

#include <cassert>

namespace obsidian::input {

void WindowEventEmitter::subscribeToWindowResizedEvent(
    WindowResizedHandler windowResizedHandler) {
  _windowResizedHandlers.emplace_back(std::move(windowResizedHandler));
}

void WindowEventEmitter::subscribeToFocusChangedEvent(
    FocusChangedHandler focusChangedHandler) {
  _focusChangedHandlers.emplace_back(std::move(focusChangedHandler));
}

void WindowEventEmitter::fireWindowResizedEvent(std::size_t newWidth,
                                                std::size_t newHeight) {
  for (auto const& handler : _windowResizedHandlers) {
    assert(handler);
    handler(newWidth, newHeight);
  }
}

void WindowEventEmitter::fireFocusChangedEvent(bool hasFocus) {
  for (auto const& handler : _focusChangedHandlers) {
    assert(handler);
    handler(hasFocus);
  }
}

void WindowEventEmitter::cleanup() {
  _windowResizedHandlers.clear();
  _focusChangedHandlers.clear();
}

} /*namespace obsidian::input*/
