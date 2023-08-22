#include <input/mouse_motion_emitter.hpp>

#include <cassert>
#include <utility>

using namespace obsidian::input;

void MouseMotionEmitter::subscribeToMouseMotionEvent(
    MouseMotionHandlerFunc onMouseMotion) {
  _mouseMotionHandlers.emplace_back(std::move(onMouseMotion));
}

void MouseMotionEmitter::fireMouseMotionEvent(std::int32_t deltaXPixel,
                                              std::int32_t deltaYPixel) {
  for (auto const& handler : _mouseMotionHandlers) {
    assert(handler);

    handler(deltaXPixel, deltaYPixel);
  }
}

void MouseMotionEmitter::cleanup() { _mouseMotionHandlers.clear(); }
