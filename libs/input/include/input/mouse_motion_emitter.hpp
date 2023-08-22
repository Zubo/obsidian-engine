#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace obsidian::input {

class MouseMotionEmitter {
public:
  using MouseMotionHandlerFunc =
      std::function<void(/*deltaXPixel=*/std::int32_t,
                         /*deltaYPixel=*/std::int32_t)>;

  MouseMotionEmitter() = default;
  MouseMotionEmitter(MouseMotionEmitter const& other) = delete;

  void subscribeToMouseMotionEvent(MouseMotionHandlerFunc onMouseMotion);

  void fireMouseMotionEvent(std::int32_t deltaXPixel, std::int32_t deltaYPixel);

  void cleanup();

private:
  std::vector<MouseMotionHandlerFunc> _mouseMotionHandlers;
};

} /*namespace obsidian::input*/
