#pragma once

#include <core/keycode.hpp>

#include <cstdint>
#include <variant>

namespace obsidian::window {

enum class WindowEventType {
  Unknown = 0,

  KeyDown = 1,
  KeyUp = 2,
  MouseMotion = 3,
  WindowResized = 4,
  ShouldQuit = 1000
};

struct KeyDownEvent {
  WindowEventType type;
  core::KeyCode keyCode;
};

struct KeyUpEvent {
  WindowEventType type;
  core::KeyCode keyCode;
};

struct MouseMotionEvent {
  WindowEventType type;
  std::int32_t deltaXPixel;
  std::int32_t deltaYPixel;
};

struct WindowResizedEvent {
  WindowEventType type;
  std::int32_t width;
  std::int32_t height;
};

struct ShouldQuitEvent {
  WindowEventType type;
};

union WindowEvent {
  WindowEventType type;
  KeyDownEvent keyDownEvent;
  KeyUpEvent keyUpEvent;
  MouseMotionEvent mouseMotionEvent;
  WindowResizedEvent windowResized;
  ShouldQuitEvent shouldQuitEvent;
};

} /*namespace obsidian::window*/
