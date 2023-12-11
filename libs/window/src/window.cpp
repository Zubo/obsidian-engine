#include <obsidian/core/keycode.hpp>
#include <obsidian/input/input_context.hpp>
#include <obsidian/input/key_input_emitter.hpp>
#include <obsidian/input/mouse_event_emitter.hpp>
#include <obsidian/window/window.hpp>
#include <obsidian/window/window_events.hpp>

#include <cassert>

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
      case WindowEventType::shouldQuit:
        _shouldQuit = true;
        break;
      case WindowEventType::keyDown:
        _inputContext->keyInputEmitter.fireKeyPressedEvent(
            e.keyDownEvent.keyCode);
        break;
      case WindowEventType::keyUp:
        _inputContext->keyInputEmitter.fireKeyReleasedEvent(
            e.keyUpEvent.keyCode);
        break;
      case WindowEventType::mouseMotion:
        _inputContext->mouseEventEmitter.fireMouseMotionEvent(
            e.mouseMotionEvent.deltaXPixel, e.mouseMotionEvent.deltaYPixel);
        break;
      case WindowEventType::mouseButtonDown:
        _inputContext->mouseEventEmitter.fireMouseButtonDownEvent(
            e.mouseButtonDownEvent.button);
        break;
      case WindowEventType::mouseButtonUp:
        _inputContext->mouseEventEmitter.fireMouseButtonUpEvent(
            e.mouseButtonUpEvent.button);
        break;
      case WindowEventType::windowResized:
        _inputContext->windowEventEmitter.fireWindowResizedEvent(
            e.windowResized.width, e.windowResized.height);
        break;
      default:
        break;
      }
    }

    if (e.type == WindowEventType::focusGainedEvent) {
      _inputContext->windowEventEmitter.fireFocusChangedEvent(true);
    } else if (e.type == WindowEventType::focusLostEvent) {
      _inputContext->windowEventEmitter.fireFocusChangedEvent(false);
    }
  }
}

bool Window::shouldQuit() const { return _shouldQuit; }

interface::IWindowBackend& Window::getWindowBackend() {
  return *_windowBackend;
}
