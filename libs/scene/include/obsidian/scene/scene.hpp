#pragma once

#include <obsidian/input/input_context.hpp>
#include <obsidian/input/key_input_emitter.hpp>
#include <obsidian/input/mouse_event_emitter.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/runtime_resource/runtime_resource_manager.hpp>
#include <obsidian/scene/camera.hpp>
#include <obsidian/scene/game_object.hpp>
#include <obsidian/serialization/game_object_data_serialization.hpp>
#include <obsidian/serialization/scene_data_serialization.hpp>

#include <glm/glm.hpp>

#include <list>
#include <memory>

namespace obsidian::rhi {

class RHI;

} /*namespace obsidian::rhi*/

namespace obsidian::runtime_resource {

class RuntimeResourceManager;

} /*namespace obsidian::runtime_resource*/

namespace obsidian::input {

struct InputContext;

} // namespace obsidian::input

namespace obsidian::scene {

struct SceneState {
  glm::vec3 ambientColor;

  serialization::CameraData camera;
  std::list<GameObject> gameObjects;
};

class Scene {
public:
  void init(input::InputContext& inputContext, rhi::RHI& rhi,
            runtime_resource::RuntimeResourceManager& resourceManager);
  SceneState const& getState() const;
  void setAmbientColor(glm::vec3 ambientColor);
  serialization::SceneData getData() const;
  void loadFromData(serialization::SceneData const& sceneData);
  GameObject&
  createGameObject(serialization::GameObjectData const& gameObjectData = {});
  std::list<GameObject>& getGameObjects();
  std::list<GameObject> const& getGameObjects() const;
  void destroyGameObject(GameObject::GameObjectId id);
  void destroyAllGameObjects();
  void resetState();

private:
  SceneState _state = {};
  bool _leftClickDown = false;
  rhi::RHI* _rhi;
  runtime_resource::RuntimeResourceManager* _resourceManager;
};

} /*namespace obsidian::scene*/
