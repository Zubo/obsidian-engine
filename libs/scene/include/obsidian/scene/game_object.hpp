#pragma once

#include <obsidian/runtime_resource/runtime_resource.hpp>

#include <glm/glm.hpp>

#include <vector>

namespace obsidian::scene {

class GameObject {
public:
  glm::vec3 const& getPosition() const;
  void setPosition(glm::vec3 const& pos);

  glm::vec3 const& getEuler() const;
  void setEuler(glm::vec3 const& euler);

  glm::vec3 const& getScale() const;
  void setScale(glm::vec3 const& scale);

  glm::mat4 const& getTransform() const;

  GameObject& createChild();

  std::vector<GameObject> const& getChildren() const;
  std::vector<GameObject>& getChildren();

  runtime_resource::RuntimeResource* materialResource = nullptr;
  runtime_resource::RuntimeResource* meshResource = nullptr;

private:
  void updateTransform();

  glm::vec3 _position;
  glm::vec3 _euler;
  glm::vec3 _scale;
  glm::mat4 _transform;

  std::vector<GameObject> _children;
};

inline void forEachGameObjAndChildren(std::vector<GameObject>& gameObjects,
                                      void (*f)(GameObject&)) {
  for (auto& obj : gameObjects) {
    f(obj);

    forEachGameObjAndChildren(obj.getChildren(), f);
  }
}

inline void
forEachGameObjAndChildren(std::vector<GameObject> const& gameObjects,
                          void (*f)(GameObject const&)) {
  for (auto& obj : gameObjects) {
    f(obj);

    forEachGameObjAndChildren(obj.getChildren(), f);
  }
}

} /*namespace obsidian::scene*/
