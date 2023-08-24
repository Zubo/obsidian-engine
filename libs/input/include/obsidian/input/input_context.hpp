#pragma once

#include <obsidian/input/key_input_emitter.hpp>
#include <obsidian/input/mouse_motion_emitter.hpp>
#include <obsidian/input/window_event_emitter.hpp>

namespace obsidian::input {

struct InputContext {
  InputContext() = default;
  InputContext(InputContext const& other) = delete;

  KeyInputEmitter keyInputEmitter;
  MouseMotionEmitter mouseMotionEmitter;
  WindowEventEmitter windowEventEmitter;
};

} /*namespace obsidian::input*/