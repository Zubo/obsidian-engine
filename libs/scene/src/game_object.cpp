#include <obsidian/core/logging.hpp>
#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/runtime_resource/runtime_resource.hpp>
#include <obsidian/scene/game_object.hpp>
#include <obsidian/serialization/game_object_data_serialization.hpp>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include <algorithm>

using namespace obsidian;
using namespace obsidian::scene;

namespace fs = std::filesystem;

GameObject::GameObjectId GameObject::_idCounter = 0;

GameObject::GameObject(
    rhi::RHI& rhi, runtime_resource::RuntimeResourceManager& resourceManager)
    : _objectId{_idCounter++}, _rhi{rhi}, _resourceManager{resourceManager} {
  _name = "GameObject" + std::to_string(_objectId);
}

GameObject::~GameObject() {
  releaseMeshResource();
  releaseMaterialResources();
  removeEnvironmentMap();
}

std::string_view GameObject::getName() const { return _name; }

void GameObject::setName(std::string_view name) { _name = name; }

void GameObject::setMaterials(
    std::vector<std::filesystem::path> const& materialResourceRelativePaths) {
  releaseMaterialResources();

  _materialRelativePaths = materialResourceRelativePaths;
}

std::vector<std::filesystem::path> const&
GameObject::getMaterialRelativePaths() {
  return _materialRelativePaths;
}

void GameObject::setMesh(std::filesystem::path meshRelativePath) {
  releaseMeshResource();

  _meshResourceRelativePath = meshRelativePath;
}

std::filesystem::path const& GameObject::getMeshRelativePath() {
  return _meshResourceRelativePath;
}

void GameObject::setDirectionalLight(
    core::DirectionalLight const& directionalLight) {
  _directionalLight = directionalLight;
}

std::optional<core::DirectionalLight> GameObject::getDirectionalLight() const {
  return _directionalLight;
}

void GameObject::removeDirectionalLight() { _directionalLight.reset(); }

void GameObject::setSpotlight(core::Spotlight const& spotlight) {
  _spotlight = spotlight;
}

std::optional<core::Spotlight> GameObject::getSpotlight() const {
  return _spotlight;
}

void GameObject::removeSpotlight() { _spotlight.reset(); }

void GameObject::setEnvironmentMap(float radius) {
  _envMapRadius = radius;

  if (_envMapResourceId == rhi::rhiIdUninitialized) {
    _envMapResourceId = _rhi.createEnvironmentMap(_position, *_envMapRadius);
  } else {
    updateEnvironmentMap();
  }
}

float GameObject::getEnvironmentMapRadius() const {
  if (!hasEnvironmentMap()) {
    OBS_LOG_ERR(
        "Member function getEnvironmentMapRadius called on a game object "
        "that doens't have environment map.");

    return -1.0f;
  }

  return *_envMapRadius;
}

bool GameObject::hasEnvironmentMap() const {
  return (_envMapResourceId != rhi::rhiIdUninitialized) && _envMapRadius;
}

void GameObject::updateEnvironmentMap() {
  if (!hasEnvironmentMap()) {
    OBS_LOG_ERR("Member function updateEnvironmentMap called on a game object "
                "that doens't have environment map.");
  }

  _rhi.updateEnvironmentMap(_envMapResourceId, _position, *_envMapRadius);
}

void GameObject::removeEnvironmentMap() {
  if (hasEnvironmentMap()) {
    _rhi.releaseEnvironmentMap(_envMapResourceId);
    _envMapResourceId = rhi::rhiIdUninitialized;
    _envMapRadius.reset();
  }
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

GameObject& GameObject::createChild() {
  GameObject& child = _children.emplace_back(_rhi, _resourceManager);
  child._parent = this;

  return child;
}

GameObject* GameObject::getParent() { return _parent; }

void GameObject::destroyChild(GameObjectId id) {
  auto const childIter =
      std::find_if(_children.cbegin(), _children.cend(),
                   [id](auto const& child) { return child.getId() == id; });
  if (childIter != _children.cend()) {
    _children.erase(childIter);
  }
}

std::list<GameObject> const& GameObject::getChildren() const {
  return _children;
}

std::list<GameObject>& GameObject::getChildren() { return _children; }

serialization::GameObjectData GameObject::getGameObjectData() const {
  serialization::GameObjectData result = {};

  result.gameObjectName = _name;

  std::transform(_materialRelativePaths.cbegin(), _materialRelativePaths.cend(),
                 std::back_inserter(result.materialPaths),
                 [](fs::path const& matRes) { return matRes; });

  result.meshPath = _meshResourceRelativePath;

  result.directionalLight = _directionalLight;
  result.spotlight = _spotlight;
  result.envMapRadius = _envMapRadius;
  result.position = getPosition();
  result.euler = getEuler();
  result.scale = getScale();

  result.children.reserve(_children.size());

  for (auto const& child : _children) {
    result.children.push_back(child.getGameObjectData());
  }

  return result;
}

void GameObject::draw(glm::mat4 const& parentTransform) {
  glm::mat4 transform = parentTransform * getTransform();

  bool meshReady = false;

  if (!_meshResourceRelativePath.empty()) {
    runtime_resource::RuntimeResource& meshResource =
        _resourceManager.getResource(_meshResourceRelativePath);

    if (meshResource.getResourceState() ==
        runtime_resource::RuntimeResourceState::initial) {
      meshResource.uploadToRHI();
    } else if (meshResource.isResourceReady()) {
      meshReady = true;
    }
  }

  bool materialsReady = true;

  if (_materialRelativePaths.size()) {
    for (auto& matRelativePath : _materialRelativePaths) {
      runtime_resource::RuntimeResource& matResource =
          _resourceManager.getResource(matRelativePath);
      if (matResource.getResourceState() ==
          runtime_resource::RuntimeResourceState::initial) {
        matResource.uploadToRHI();
      }

      materialsReady &= matResource.isResourceReady();
    }
  }

  if (meshReady && materialsReady) {
    rhi::DrawCall drawCall;
    for (auto const& materialResourcePath : _materialRelativePaths) {
      drawCall.materialIds.push_back(
          _resourceManager.getResource(materialResourcePath).getResourceId());
    }

    drawCall.meshId =
        _resourceManager.getResource(_meshResourceRelativePath).getResourceId();
    drawCall.transform = transform;
    _rhi.submitDrawCall(drawCall);
  }

  if (_directionalLight) {
    core::DirectionalLight directionalLight = *_directionalLight;
    directionalLight.direction = glm::normalize(
        transform * glm::vec4{0.0f, 0.0f, 1.0f, /*no translation:*/ 0.0f});
    _rhi.submitLight(directionalLight);
  }

  if (_spotlight) {
    core::Spotlight spotlight = *_spotlight;
    spotlight.position = getPosition();
    spotlight.direction = glm::normalize(
        transform * glm::vec4{0.0f, 0.0f, 1.0f, /*no translation*/ 0.0f});
    _rhi.submitLight(spotlight);
  }

  for (auto& child : getChildren()) {
    child.draw(transform);
  }
}

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

void GameObject::releaseMaterialResources() {
  if (_materialRelativePaths.size()) {
    for (fs::path const& matRelativePath : _materialRelativePaths) {
      _resourceManager.getResource(matRelativePath).releaseFromRHI();
    }
  }
}

void GameObject::releaseMeshResource() {
  if (!_meshResourceRelativePath.empty()) {
    _resourceManager.getResource(_meshResourceRelativePath).releaseFromRHI();
  }
}
