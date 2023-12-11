#pragma once

#include <obsidian/input/input_context.hpp>
#include <obsidian/input/key_input_emitter.hpp>
#include <obsidian/input/mouse_event_emitter.hpp>
#include <obsidian/scene/camera.hpp>
#include <obsidian/scene/game_object.hpp>

#include <glm/glm.hpp>

#include <deque>

namespace obsidian::input {

struct InputContext;

} // namespace obsidian::input

namespace obsidian::scene {

struct SceneState {
  glm::vec3 ambientColor;

  Camera camera;
  // TODO: use better
  std::deque<GameObject> gameObjects;
};

class Scene {
public:
  void init(input::InputContext& inputContext);
  SceneState& getState();
  SceneState const& getState() const;

private:
  SceneState _state = {};
  bool _leftClickDown = false;
};

} /*namespace obsidian::scene*/
