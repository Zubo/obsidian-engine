#include <glm/gtx/transform.hpp>
#include <obsidian/scene/game_object.hpp>

using namespace obsidian::scene;

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

  _transform *= glm::scale(_scale);

  _transform *=
      glm::rotate(glm::radians(_euler.x), glm::vec3{1.0f, 0.0f, 0.0f});
  _transform *=
      glm::rotate(glm::radians(_euler.y), glm::vec3{0.0f, 1.0f, 0.0f});
  _transform *=
      glm::rotate(glm::radians(_euler.z), glm::vec3{0.0f, 0.0f, 1.0f});

  _transform *= glm::translate(_position);
}

GameObject& GameObject::createChild() { return _children.emplace_back(); }

std::vector<GameObject> const& GameObject::getChildren() const {
  return _children;
}

std::vector<GameObject>& GameObject::getChildren() { return _children; }
