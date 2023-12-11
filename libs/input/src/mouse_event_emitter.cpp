#include <obsidian/input/mouse_event_emitter.hpp>

#include <cassert>
#include <utility>

using namespace obsidian::input;

void MouseEventEmitter::subscribeToMouseMotionEvent(
    MouseMotionHandlerFunc onMouseMotion) {
  _mouseMotionHandlers.emplace_back(std::move(onMouseMotion));
}

void MouseEventEmitter::fireMouseMotionEvent(std::int32_t deltaXPixel,
                                             std::int32_t deltaYPixel) {
  for (auto const& handler : _mouseMotionHandlers) {
    assert(handler);

    handler(deltaXPixel, deltaYPixel);
  }
}

void MouseEventEmitter::subscribeToMouseButtonDownEvent(
    MouseButtonHandlerFunc onMouseButtonDown) {
  _mouseButtonDownHandlers.emplace_back(std::move(onMouseButtonDown));
}

void MouseEventEmitter::fireMouseButtonDownEvent(
    core::MouseButtonType mouseButtonType) {
  for (auto const& handler : _mouseButtonDownHandlers) {
    assert(handler);

    handler(mouseButtonType);
  }
}

void MouseEventEmitter::subscribeToMouseButtonUpEvent(
    MouseButtonHandlerFunc onMouseButtonUp) {
  _mouseButtonUpHandlers.emplace_back(std::move(onMouseButtonUp));
}

void MouseEventEmitter::fireMouseButtonUpEvent(
    core::MouseButtonType mouseButtonType) {
  for (auto const& handler : _mouseButtonUpHandlers) {
    assert(handler);

    handler(mouseButtonType);
  }
}

void MouseEventEmitter::cleanup() {
  _mouseMotionHandlers.clear();
  _mouseButtonDownHandlers.clear();
  _mouseButtonUpHandlers.clear();
}
