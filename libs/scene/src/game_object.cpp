#include <glm/gtx/transform.hpp>

#include <obsidian/scene/game_object.hpp>

#include <algorithm>

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
  GameObject& child = _children.emplace_back();
  child.parent = this;

  return child;
}

GameObject* GameObject::getParent() { return parent; }

void GameObject::destroyChild(GameObjectId id) {
  auto const childIter =
      std::find_if(_children.cbegin(), _children.cend(),
                   [id](auto const& child) { return child.getId() == id; });
  if (childIter != _children.cend()) {
    _children.erase(childIter);
  }
}

std::deque<GameObject> const& GameObject::getChildren() const {
  return _children;
}

std::deque<GameObject>& GameObject::getChildren() { return _children; }
