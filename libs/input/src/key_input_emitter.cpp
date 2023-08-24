#include <obsidian/input/key_input_emitter.hpp>

#include <cassert>

using namespace obsidian::input;

void KeyInputEmitter::subscribeToKeycodePressed(KeyHandlerFunc onKeyPressed,
                                                core::KeyCode keyCode) {

  _keyPressedHandler[keyCode].emplace_back(std::move(onKeyPressed));
}

void KeyInputEmitter::subscribeToKeycodeReleased(KeyHandlerFunc onKeyPressed,
                                                 core::KeyCode keyCode) {
  _keyReleasedHandler[keyCode].emplace_back(std::move(onKeyPressed));
}

void KeyInputEmitter::fireKeyPressedEvent(core::KeyCode key) {
  for (auto const& handler : _keyPressedHandler[key]) {
    assert(handler);

    handler();
  }
}

void KeyInputEmitter::fireKeyReleasedEvent(core::KeyCode key) {
  for (auto const& handler : _keyReleasedHandler[key]) {
    assert(handler);

    handler();
  }
}

void KeyInputEmitter::cleanup() {
  _keyPressedHandler.clear();
  _keyReleasedHandler.clear();
}
