#include <obsidian/input/input_context.hpp>
#include <obsidian/input/key_input_emitter.hpp>
#include <obsidian/input/mouse_motion_emitter.hpp>
#include <obsidian/scene/scene.hpp>

#include <glm/gtx/transform.hpp>

using namespace obsidian::scene;

void Scene::init(input::InputContext& inputContext) {
  // scene movement
  input::KeyInputEmitter& keyInputEmitter = inputContext.keyInputEmitter;

  constexpr glm::vec3 worldY = {0.0f, 1.0f, 0.0f};
  keyInputEmitter.subscribeToKeycodePressed(
      [this]() { _state.camera.pos += _state.camera.forward(); },
      core::KeyCode::e);
  keyInputEmitter.subscribeToKeycodePressed(
      [this]() { _state.camera.pos -= _state.camera.forward(); },
      core::KeyCode::d);
  keyInputEmitter.subscribeToKeycodePressed(
      [this]() { _state.camera.pos -= _state.camera.right(); },
      core::KeyCode::s);
  keyInputEmitter.subscribeToKeycodePressed(
      [this]() { _state.camera.pos += _state.camera.right(); },
      core::KeyCode::f);
  keyInputEmitter.subscribeToKeycodePressed(
      [this, worldY]() { _state.camera.pos += worldY; }, core::KeyCode::r);
  keyInputEmitter.subscribeToKeycodePressed(
      [this, worldY]() { _state.camera.pos -= worldY; }, core::KeyCode::w);

  // camera rotation
  input::MouseMotionEmitter& mouseMotionEmitter =
      inputContext.mouseMotionEmitter;

  mouseMotionEmitter.subscribeToMouseMotionEvent(
      [this](std::int32_t mouseDeltaXPixel, std::int32_t mouseDeltaYPixel) {
        constexpr float pi = 3.14f;
        constexpr float camMotionFactor = 0.01f;

        _state.camera.rotationRad +=
            camMotionFactor * glm::vec2{-mouseDeltaYPixel, -mouseDeltaXPixel};
        _state.camera.rotationRad.x =
            glm::clamp(_state.camera.rotationRad.x, -0.5f * pi, 0.5f * pi);
      });
}

SceneState& Scene::getState() { return _state; }
SceneState const& Scene::getState() const { return _state; }
