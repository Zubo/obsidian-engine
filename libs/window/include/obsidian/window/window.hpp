#pragma once

#include <obsidian/input/input_context.hpp>
#include <obsidian/window/window_backend.hpp>
#include <obsidian/window/window_events.hpp>

#include <memory>

struct SDL_Window;

namespace obsidian::input {

class InputContext;

} /*namespace obsidian::input*/

namespace obsidian::window {

class Window {
public:
  void init(std::unique_ptr<interface::IWindowBackend> windowBackendUnique,
            input::InputContext& inputContext);

  void pollEvents();

  bool shouldQuit() const;
  interface::IWindowBackend& getWindowBackend();

private:
  std::unique_ptr<interface::IWindowBackend> _windowBackend = nullptr;
  input::InputContext* _inputContext = nullptr;
  bool _shouldQuit = false;
  bool _hasFocus = false;
  std::vector<WindowEvent> _polledEvents;
};

} /*namespace obsidian::window*/
