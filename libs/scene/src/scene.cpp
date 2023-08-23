#include <input/input_context.hpp>
#include <input/key_input_emitter.hpp>
#include <input/mouse_motion_emitter.hpp>
#include <scene/scene.hpp>

#include <glm/gtx/transform.hpp>

using namespace obsidian::scene;

void Scene::init(input::InputContext& inputContext) {
  // scene movement
  input::KeyInputEmitter& keyInputEmitter = inputContext.keyInputEmitter;

  constexpr glm::vec3 worldY = {0.0f, 1.0f, 0.0f};
  keyInputEmitter.subscribeToKeycodePressed(
      [this]() { _state.cameraPos += getCameraForward(); }, core::KeyCode::e);
  keyInputEmitter.subscribeToKeycodePressed(
      [this]() { _state.cameraPos -= getCameraForward(); }, core::KeyCode::d);
  keyInputEmitter.subscribeToKeycodePressed(
      [this]() { _state.cameraPos -= getCameraRight(); }, core::KeyCode::s);
  keyInputEmitter.subscribeToKeycodePressed(
      [this]() { _state.cameraPos += getCameraRight(); }, core::KeyCode::f);
  keyInputEmitter.subscribeToKeycodePressed(
      [this, worldY]() { _state.cameraPos += worldY; }, core::KeyCode::r);
  keyInputEmitter.subscribeToKeycodePressed(
      [this, worldY]() { _state.cameraPos -= worldY; }, core::KeyCode::w);

  // camera rotation
  input::MouseMotionEmitter& mouseMotionEmitter =
      inputContext.mouseMotionEmitter;

  mouseMotionEmitter.subscribeToMouseMotionEvent(
      [this](std::int32_t mouseDeltaXPixel, std::int32_t mouseDeltaYPixel) {
        constexpr float pi = 3.14f;
        constexpr float camMotionFactor = 0.01f;

        _state.cameraRotationRad +=
            camMotionFactor * glm::vec2{-mouseDeltaYPixel, -mouseDeltaXPixel};
        _state.cameraRotationRad.x =
            glm::clamp(_state.cameraRotationRad.x, -0.5f * pi, 0.5f * pi);
      });
}

SceneState& Scene::getState() { return _state; }
SceneState const& Scene::getState() const { return _state; }

glm::vec3 Scene::getCameraForward() const {
  glm::vec3 constexpr worldX{1.f, 0.f, 0.0f};
  glm::vec3 constexpr worldY{0.f, 1.f, 0.f};
  glm::vec3 constexpr worldZ{0.f, 0.f, 1.f};

  glm::vec3 const cameraForward =
      glm::normalize(glm::rotate(_state.cameraRotationRad.y, worldY) *
                     glm::rotate(_state.cameraRotationRad.x, worldX) *
                     glm::vec4{-worldZ, 1.f});

  return cameraForward;
}

glm::vec3 Scene::getCameraRight() const {
  glm::vec3 constexpr worldY{0.f, 1.f, 0.f};

  glm::vec3 const cameraForward = getCameraForward();
  glm::vec3 const cameraRight =
      glm::normalize(glm::cross(cameraForward, worldY));

  return cameraRight;
}
