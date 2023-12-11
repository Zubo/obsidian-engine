#pragma once

#include <obsidian/input/key_input_emitter.hpp>
#include <obsidian/input/mouse_event_emitter.hpp>
#include <obsidian/input/window_event_emitter.hpp>

namespace obsidian::input {

struct InputContext {
  InputContext() = default;
  InputContext(InputContext const& other) = delete;

  KeyInputEmitter keyInputEmitter;
  MouseEventEmitter mouseEventEmitter;
  WindowEventEmitter windowEventEmitter;
};

} /*namespace obsidian::input*/
