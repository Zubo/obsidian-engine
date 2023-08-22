#include <input/window_event_emitter.hpp>

#include <cassert>

namespace obsidian::input {

void WindowEventEmitter::subscribeToWindowResizedEvent(
    WindowResizedHandler windowResizedHandler) {
  _windowResizedHandlers.emplace_back(std::move(windowResizedHandler));
}

void WindowEventEmitter::fireWindowResizedEvent(std::size_t newWidth,
                                                std::size_t newHeight) {
  for (auto const& handler : _windowResizedHandlers) {
    assert(handler);
    handler(newWidth, newHeight);
  }
}

void WindowEventEmitter::cleanup() { _windowResizedHandlers.clear(); }

} /*namespace obsidian::input*/
