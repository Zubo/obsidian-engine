#include <obsidian/core/keycode.hpp>
#include <obsidian/input/input_context.hpp>
#include <obsidian/input/key_input_emitter.hpp>
#include <obsidian/input/mouse_event_emitter.hpp>
#include <obsidian/scene/game_object.hpp>
#include <obsidian/scene/scene.hpp>
#include <obsidian/serialization/game_object_data_serialization.hpp>
#include <obsidian/serialization/scene_data_serialization.hpp>

#include <glm/gtx/transform.hpp>

using namespace obsidian::scene;

void Scene::init(input::InputContext& inputContext, rhi::RHI& rhi,
                 runtime_resource::RuntimeResourceManager& resourceManager) {
  // scene movement
  _rhi = &rhi;
  _resourceManager = &resourceManager;

  input::KeyInputEmitter& keyInputEmitter = inputContext.keyInputEmitter;

  constexpr glm::vec3 worldY = {0.0f, 1.0f, 0.0f};
  constexpr float moveSpeed{0.1f};
  keyInputEmitter.subscribeToKeycodePressed(
      [this]() { _state.camera.pos += moveSpeed * forward(_state.camera); },
      core::KeyCode::e);
  keyInputEmitter.subscribeToKeycodePressed(
      [this]() { _state.camera.pos -= moveSpeed * forward(_state.camera); },
      core::KeyCode::d);
  keyInputEmitter.subscribeToKeycodePressed(
      [this]() { _state.camera.pos -= moveSpeed * right(_state.camera); },
      core::KeyCode::s);
  keyInputEmitter.subscribeToKeycodePressed(
      [this]() { _state.camera.pos += moveSpeed * right(_state.camera); },
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

SceneState const& Scene::getState() const { return _state; }

void Scene::setAmbientColor(glm::vec3 ambientColor) {
  _state.ambientColor = ambientColor;
}

obsidian::serialization::SceneData Scene::getData() const {
  serialization::SceneData sceneData = {};
  sceneData.ambientColor = _state.ambientColor;
  sceneData.camera = _state.camera;

  for (GameObject const& gameObject : _state.gameObjects) {
    sceneData.gameObjects.push_back(gameObject.getGameObjectData());
  }

  return sceneData;
}

void Scene::loadFromData(serialization::SceneData const& sceneData) {
  _state.ambientColor = sceneData.ambientColor;
  _state.camera = sceneData.camera;

  for (serialization::GameObjectData const& gameObjData :
       sceneData.gameObjects) {
    scene::GameObject& gameObject =
        _state.gameObjects.emplace_back(*_rhi, *_resourceManager);
    gameObject.populate(gameObjData);
  }
}

GameObject&
Scene::createGameObject(serialization::GameObjectData const& gameObjectData) {
  GameObject& gameObject =
      _state.gameObjects.emplace_back(*_rhi, *_resourceManager);
  gameObject.populate(gameObjectData);

  return gameObject;
}

std::list<GameObject>& Scene::getGameObjects() { return _state.gameObjects; }

std::list<GameObject> const& Scene::getGameObjects() const {
  return _state.gameObjects;
}

void Scene::destroyGameObject(GameObject::GameObjectId id) {
  auto const gameObjectIter =
      std::find_if(_state.gameObjects.cbegin(), _state.gameObjects.cend(),
                   [id](auto const& g) { return id == g.getId(); });

  if (gameObjectIter != _state.gameObjects.cend()) {
    _state.gameObjects.erase(gameObjectIter);
  }
}

void Scene::destroyAllGameObjects() { _state.gameObjects.clear(); }

void Scene::resetState() { _state = {}; }
