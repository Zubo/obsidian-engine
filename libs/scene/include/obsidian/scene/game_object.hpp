#pragma once

#include <obsidian/core/light_types.hpp>
#include <obsidian/runtime_resource/runtime_resource.hpp>
#include <obsidian/serialization/game_object_data_serialization.hpp>

#include <glm/glm.hpp>

#include <deque>
#include <optional>
#include <string>

namespace obsidian::scene {

class GameObject {
public:
  using GameObjectId = std::size_t;

  GameObject();
  GameObject(GameObject const& other) = delete;
  GameObject(GameObject&& other) noexcept = default;
  GameObject& operator=(GameObject&& other) noexcept = default;

  GameObjectId getId() const;

  glm::vec3 const& getPosition() const;
  void setPosition(glm::vec3 const& pos);

  glm::vec3 const& getEuler() const;
  void setEuler(glm::vec3 const& euler);

  glm::vec3 const& getScale() const;
  void setScale(glm::vec3 const& scale);

  glm::mat4 const& getTransform() const;

  GameObject& createChild();

  void addChild(GameObject&& gameObject);

  GameObject* getParent();

  void destroyChild(GameObjectId id);

  std::deque<GameObject> const& getChildren() const;
  std::deque<GameObject>& getChildren();

  serialization::GameObjectData getGameObjectData() const;

  std::string name;
  std::vector<runtime_resource::RuntimeResource*> materialResources;
  runtime_resource::RuntimeResource* meshResource = nullptr;
  std::optional<core::DirectionalLight> directionalLight;
  std::optional<core::Spotlight> spotlight;
  GameObject* parent = nullptr;

private:
  void updateTransform();

  GameObjectId _objectId;
  glm::vec3 _position = {};
  glm::vec3 _euler = {};
  glm::vec3 _scale = {1.0f, 1.0f, 1.0f};
  glm::mat4 _transform{1.0f};

  // TODO: use better data structure to store GameObjects
  std::deque<GameObject> _children;

  static GameObjectId _idCounter;
};

inline void forEachGameObjAndChildren(std::deque<GameObject>& gameObjects,
                                      void (*f)(GameObject&)) {
  for (auto& obj : gameObjects) {
    f(obj);

    forEachGameObjAndChildren(obj.getChildren(), f);
  }
}

inline void forEachGameObjAndChildren(std::deque<GameObject> const& gameObjects,
                                      void (*f)(GameObject const&)) {
  for (auto& obj : gameObjects) {
    f(obj);

    forEachGameObjAndChildren(obj.getChildren(), f);
  }
}

} /*namespace obsidian::scene*/
