#include <obsidian/scene/game_object.hpp>
#include <obsidian/serialization/game_object_data_serialization.hpp>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include <algorithm>

using namespace obsidian;
using namespace obsidian::scene;

GameObject::GameObjectId GameObject::_idCounter = 0;

GameObject::GameObject() : _objectId{_idCounter++} {
  name = "GameObject" + std::to_string(_objectId);
}

GameObject::GameObjectId GameObject::getId() const { return _objectId; }

glm::vec3 const& GameObject::getPosition() const { return _position; }

void GameObject::setPosition(glm::vec3 const& pos) {
  _position = pos;
  updateTransform();
}

glm::vec3 const& GameObject::getEuler() const { return _euler; }

void GameObject::setEuler(glm::vec3 const& euler) {
  _euler = euler;
  updateTransform();
}

glm::vec3 const& GameObject::getScale() const { return _scale; }

void GameObject::setScale(glm::vec3 const& scale) {
  _scale = scale;
  updateTransform();
}

glm::mat4 const& GameObject::getTransform() const { return _transform; }

void GameObject::updateTransform() {
  _transform = glm::mat4{1.0f};

  _transform *= glm::translate(_position);

  _transform *=
      glm::rotate(glm::radians(_euler.x), glm::vec3{1.0f, 0.0f, 0.0f});
  _transform *=
      glm::rotate(glm::radians(_euler.y), glm::vec3{0.0f, 1.0f, 0.0f});
  _transform *=
      glm::rotate(glm::radians(_euler.z), glm::vec3{0.0f, 0.0f, 1.0f});

  _transform *= glm::scale(_scale);
}

GameObject& GameObject::createChild() {
  GameObject& child = *_children.emplace_back(std::make_unique<GameObject>());
  child.parent = this;

  return child;
}

GameObject* GameObject::getParent() { return parent; }

void GameObject::destroyChild(GameObjectId id) {
  auto const childIter =
      std::find_if(_children.cbegin(), _children.cend(),
                   [id](auto const& child) { return child->getId() == id; });
  if (childIter != _children.cend()) {
    _children.erase(childIter);
  }
}

std::vector<std::unique_ptr<GameObject>> const&
GameObject::getChildren() const {
  return _children;
}

std::vector<std::unique_ptr<GameObject>>& GameObject::getChildren() {
  return _children;
}

serialization::GameObjectData GameObject::getGameObjectData() const {
  serialization::GameObjectData result = {};

  result.gameObjectName = name;

  std::transform(materialResources.cbegin(), materialResources.cend(),
                 std::back_inserter(result.materialPaths),
                 [](auto const* matRes) { return matRes->getRelativePath(); });

  if (meshResource) {
    result.meshPath = meshResource->getRelativePath();
  }

  result.directionalLight = directionalLight;
  result.spotlight = spotlight;
  result.position = getPosition();
  result.euler = getEuler();
  result.scale = getScale();

  result.children.reserve(_children.size());

  for (auto const& child : _children) {
    result.children.push_back(child->getGameObjectData());
  }

  return result;
}
