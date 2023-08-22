#include <core/keycode.hpp>
#include <input/input_context.hpp>
#include <input/key_input_emitter.hpp>
#include <input/mouse_motion_emitter.hpp>
#include <window/window.hpp>
#include <window/window_events.hpp>

#include <cassert>
#include <iostream>

using namespace obsidian::window;

void Window::init(
    std::unique_ptr<interface::IWindowBackend> windowBackendUnique,
    input::InputContext& inputContext) {
  _windowBackend = std::move(windowBackendUnique);
  (void)windowBackendUnique;

  _inputContext = &inputContext;

  _inputContext->windowEventEmitter.subscribeToFocusChangedEvent(
      [this](bool hasFocus) { _hasFocus = hasFocus; });
}

void Window::pollEvents() {
  assert(_windowBackend);
  assert(_inputContext);

  _polledEvents.clear();
  _windowBackend->pollEvents(_polledEvents);

  for (WindowEvent const& e : _polledEvents) {
    if (_hasFocus) {
      switch (e.type) {
      case WindowEventType::ShouldQuit:
        _shouldQuit = true;
        break;
      case WindowEventType::KeyDown:
        _inputContext->keyInputEmitter.fireKeyPressedEvent(
            e.keyDownEvent.keyCode);
        break;
      case WindowEventType::KeyUp:
        _inputContext->keyInputEmitter.fireKeyReleasedEvent(
            e.keyUpEvent.keyCode);
        break;
      case WindowEventType::MouseMotion:
        _inputContext->mouseMotionEmitter.fireMouseMotionEvent(
            e.mouseMotionEvent.deltaXPixel, e.mouseMotionEvent.deltaYPixel);
        break;
      case WindowEventType::WindowResized:
        _inputContext->windowEventEmitter.fireWindowResizedEvent(
            e.windowResized.width, e.windowResized.height);
        break;
      default:
        break;
      }
    }

    if (e.type == WindowEventType::FocusGainedEvent) {
      _inputContext->windowEventEmitter.fireFocusChangedEvent(true);
    } else if (e.type == WindowEventType::FocusLostEvent) {
      _inputContext->windowEventEmitter.fireFocusChangedEvent(false);
    }
  }
}

bool Window::shouldQuit() const { return _shouldQuit; }

interface::IWindowBackend& Window::getWindowBackend() {
  return *_windowBackend;
}
