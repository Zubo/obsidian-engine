#pragma once

#include <obsidian/core/keycode.hpp>

#include <cstdint>
#include <functional>
#include <vector>

namespace obsidian::input {

class MouseEventEmitter {
public:
  using MouseMotionHandlerFunc =
      std::function<void(/*deltaXPixel=*/std::int32_t,
                         /*deltaYPixel=*/std::int32_t)>;

  using MouseButtonHandlerFunc = std::function<void(core::MouseButtonType)>;

  MouseEventEmitter() = default;
  MouseEventEmitter(MouseEventEmitter const& other) = delete;

  void subscribeToMouseMotionEvent(MouseMotionHandlerFunc onMouseMotion);

  void fireMouseMotionEvent(std::int32_t deltaXPixel, std::int32_t deltaYPixel);

  void
  subscribeToMouseButtonDownEvent(MouseButtonHandlerFunc onMouseButtonDown);

  void fireMouseButtonDownEvent(core::MouseButtonType mouseButtonType);

  void subscribeToMouseButtonUpEvent(MouseButtonHandlerFunc onMouseButtonUp);

  void fireMouseButtonUpEvent(core::MouseButtonType mouseButtonType);

  void cleanup();

private:
  std::vector<MouseMotionHandlerFunc> _mouseMotionHandlers;
  std::vector<MouseButtonHandlerFunc> _mouseButtonDownHandlers;
  std::vector<MouseButtonHandlerFunc> _mouseButtonUpHandlers;
};

} /*namespace obsidian::input*/
