#pragma once

#include <obsidian/core/keycode.hpp>

#include <array>
#include <functional>
#include <unordered_map>
#include <vector>

namespace obsidian::input {

class KeyInputEmitter {
public:
  using KeyHandlerFunc = std::function<void(void)>;

  KeyInputEmitter() = default;
  KeyInputEmitter(KeyInputEmitter const& other) = delete;

  void subscribeToKeycodePressed(KeyHandlerFunc onKeyPressed,
                                 core::KeyCode keyCode);
  void subscribeToKeycodeReleased(KeyHandlerFunc onKeyPressed,
                                  core::KeyCode keyCode);

  void fireKeyPressedEvent(core::KeyCode key);
  void fireKeyReleasedEvent(core::KeyCode key);

  void cleanup();

private:
  // map for keycodes in interval [0, 127]
  std::unordered_map<core::KeyCode, std::vector<KeyHandlerFunc>>
      _keyPressedHandler;
  std::unordered_map<core::KeyCode, std::vector<KeyHandlerFunc>>
      _keyReleasedHandler;
};

} /*namespace obsidian::input*/
