#pragma once

#include <input/input_context.hpp>
#include <input/key_input_emitter.hpp>
#include <input/mouse_motion_emitter.hpp>
#include <scene/camera.hpp>
#include <scene/game_object.hpp>

#include <glm/glm.hpp>
#include <vector>

namespace obsidian::input {

struct InputContext;

} // namespace obsidian::input

namespace obsidian::scene {

struct SceneState {
  glm::vec3 ambientColor;
  glm::vec3 sunDirection;
  glm::vec3 sunColor;

  Camera camera;
  std::vector<GameObject> gameObjects;
};

class Scene {
public:
  void init(input::InputContext& inputContext);
  SceneState& getState();
  SceneState const& getState() const;

private:
  SceneState _state = {};
};

} /*namespace obsidian::scene*/
