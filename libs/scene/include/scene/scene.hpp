#pragma once

#include <input/input_context.hpp>
#include <input/key_input_emitter.hpp>
#include <input/mouse_motion_emitter.hpp>

#include <glm/glm.hpp>

namespace obsidian::input {

struct InputContext;

} // namespace obsidian::input

namespace obsidian::scene {

struct SceneState {
  glm::vec3 cameraPos;
  glm::vec2 cameraRotationRad;

  glm::vec3 ambientColor;
  glm::vec3 sunDirection;
  glm::vec3 sunColor;
};

class Scene {
public:
  void init(input::InputContext& inputContext);
  SceneState& getState();
  SceneState const& getState() const;

private:
  glm::vec3 getCameraForward() const;
  glm::vec3 getCameraRight() const;
  SceneState _state = {};
};

} /*namespace obsidian::scene*/
