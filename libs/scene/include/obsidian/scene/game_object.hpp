#pragma once

#include <obsidian/core/light_types.hpp>
#include <obsidian/runtime_resource/runtime_resource.hpp>
#include <obsidian/serialization/game_object_data_serialization.hpp>

#include <glm/glm.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace obsidian::scene {

class GameObject {
public:
  using GameObjectId = std::size_t;
  static constexpr std::size_t invalidId = (std::size_t)-1;

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

  GameObjectId getParentId();
  GameObject* getParent();

  void destroyChild(GameObjectId id);

  std::vector<std::unique_ptr<GameObject>> const& getChildren() const;
  std::vector<std::unique_ptr<GameObject>>& getChildren();

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
  std::vector<std::unique_ptr<GameObject>> _children;

  static GameObjectId _idCounter;
};

} /*namespace obsidian::scene*/
