#include <obsidian/core/keycode.hpp>
#include <obsidian/input/input_context.hpp>
#include <obsidian/input/key_input_emitter.hpp>
#include <obsidian/input/mouse_event_emitter.hpp>
#include <obsidian/scene/scene.hpp>

#include <glm/gtx/transform.hpp>

using namespace obsidian::scene;

void Scene::init(input::InputContext& inputContext) {
  // scene movement
  input::KeyInputEmitter& keyInputEmitter = inputContext.keyInputEmitter;

  constexpr glm::vec3 worldY = {0.0f, 1.0f, 0.0f};
  constexpr float moveSpeed{0.1f};
  keyInputEmitter.subscribeToKeycodePressed(
      [this]() { _state.camera.pos += moveSpeed * _state.camera.forward(); },
      core::KeyCode::e);
  keyInputEmitter.subscribeToKeycodePressed(
      [this]() { _state.camera.pos -= moveSpeed * _state.camera.forward(); },
      core::KeyCode::d);
  keyInputEmitter.subscribeToKeycodePressed(
      [this]() { _state.camera.pos -= moveSpeed * _state.camera.right(); },
      core::KeyCode::s);
  keyInputEmitter.subscribeToKeycodePressed(
      [this]() { _state.camera.pos += moveSpeed * _state.camera.right(); },
      core::KeyCode::f);
  keyInputEmitter.subscribeToKeycodePressed(
      [this, worldY]() { _state.camera.pos += moveSpeed * worldY; },
      core::KeyCode::r);
  keyInputEmitter.subscribeToKeycodePressed(
      [this, worldY]() { _state.camera.pos -= moveSpeed * worldY; },
      core::KeyCode::w);

  // camera rotation
  input::MouseEventEmitter& mouseEventEmitter = inputContext.mouseEventEmitter;

  mouseEventEmitter.subscribeToMouseMotionEvent(
      [this](std::int32_t mouseDeltaXPixel, std::int32_t mouseDeltaYPixel) {
        if (!_leftClickDown) {
          return;
        }

        constexpr float pi = 3.14f;
        constexpr float camMotionFactor = 0.01f;

        _state.camera.rotationRad +=
            camMotionFactor * glm::vec2{-mouseDeltaYPixel, -mouseDeltaXPixel};
        _state.camera.rotationRad.x =
            glm::clamp(_state.camera.rotationRad.x, -0.5f * pi, 0.5f * pi);
      });

  mouseEventEmitter.subscribeToMouseButtonDownEvent(
      [this](core::MouseButtonType buttonType) {
        if (buttonType == core::MouseButtonType::left) {
          _leftClickDown = true;
        }
      });

  mouseEventEmitter.subscribeToMouseButtonUpEvent(
      [this](core::MouseButtonType buttonType) {
        if (buttonType == core::MouseButtonType::left) {
          _leftClickDown = false;
        }
      });
}

SceneState& Scene::getState() { return _state; }
SceneState const& Scene::getState() const { return _state; }
