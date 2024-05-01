#pragma once

#include <obsidian/core/keycode.hpp>

#include <cstdint>

namespace obsidian::window {

enum class WindowEventType {
  unknown,

  keyDown,
  keyUp,
  mouseMotion,
  mouseButtonUp,
  windowResized,
  mouseButtonDown,
  focusGainedEvent,
  focusLostEvent,
  shouldQuit
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

struct MouseButtonDownEvent {
  WindowEventType type;
  core::MouseButtonType button;
};

struct MouseButtonUpEvent {
  WindowEventType type;
  core::MouseButtonType button;
};

struct WindowResizedEvent {
  WindowEventType type;
  std::int32_t width;
  std::int32_t height;
};

struct ShouldQuitEvent {
  WindowEventType type;
};

struct FocusGainedEvent {
  WindowEventType type;
};

struct FocusLostEvent {
  WindowEventType type;
};

union WindowEvent {
  WindowEventType type;
  KeyDownEvent keyDownEvent;
  KeyUpEvent keyUpEvent;
  MouseMotionEvent mouseMotionEvent;
  MouseButtonDownEvent mouseButtonDownEvent;
  MouseButtonUpEvent mouseButtonUpEvent;
  WindowResizedEvent windowResized;
  FocusGainedEvent focusGained;
  FocusLostEvent focusLost;
  ShouldQuitEvent shouldQuitEvent;
};

} /*namespace obsidian::window*/
