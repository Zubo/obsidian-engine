#include <obsidian/core/logging.hpp>
#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/runtime_resource/runtime_resource.hpp>
#include <obsidian/scene/game_object.hpp>
#include <obsidian/serialization/game_object_data_serialization.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
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

GameObject::GameObject(GameObject&& other) noexcept
    : _rhi{other._rhi}, _resourceManager{other._resourceManager} {
  *this = std::move(other);
}

GameObject::~GameObject() { cleanup(); }

GameObject& GameObject::operator=(GameObject&& other) noexcept {
  if (this != &other) {
    cleanup();

    _name = std::move(other._name);
    _materialRelativePaths = std::move(other._materialRelativePaths);
    _meshResourceRelativePath = std::move(other._meshResourceRelativePath);
    _directionalLight = std::move(other._directionalLight);
    _spotlight = std::move(other._spotlight);
    _envMapRadius = std::move(other._envMapRadius);

    _envMapResourceId = other._envMapResourceId;
    other._envMapResourceId = rhi::rhiIdUninitialized;

    _parent = other._parent;
    other._parent = nullptr;

    _children = std::move(other._children);

    _objectId = other._objectId;
    other._objectId = invalidId;

    _position = other._position;
    _rotationQuat = other._rotationQuat;
    _scale = other._scale;
    _transform = other._transform;
    _rhi = other._rhi;
    _resourceManager = other._resourceManager;
  }

  return *this;
}

std::string_view GameObject::getName() const { return _name; }

void GameObject::setName(std::string_view name) { _name = name; }

void GameObject::setMaterials(
    std::vector<std::filesystem::path> const& materialResourceRelativePaths) {
  releaseMaterialResources();

  _materialRelativePaths = materialResourceRelativePaths;

  for (std::filesystem::path const& p : _materialRelativePaths) {
    _resourceManager.get().getResource(p).declareRef();
  }
}

std::vector<std::filesystem::path> const&
GameObject::getMaterialRelativePaths() {
  return _materialRelativePaths;
}

void GameObject::setMesh(std::filesystem::path meshRelativePath) {
  releaseMeshResource();

  _meshResourceRelativePath = meshRelativePath;
  _resourceManager.get().getResource(_meshResourceRelativePath).declareRef();
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
    _envMapResourceId =
        _rhi.get().createEnvironmentMap(_position, *_envMapRadius);
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

  _rhi.get().updateEnvironmentMap(_envMapResourceId, _position, *_envMapRadius);
}

void GameObject::removeEnvironmentMap() {
  if (hasEnvironmentMap()) {
    _rhi.get().releaseEnvironmentMap(_envMapResourceId);
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

glm::vec3 GameObject::getEuler() const {
  glm::vec3 euler = glm::degrees(glm::eulerAngles(_rotationQuat));
  return euler;
}

void GameObject::setEuler(glm::vec3 const& euler) {
  _rotationQuat = glm::quat(glm::radians(euler));

  updateTransform();
}

glm::quat const& GameObject::getRotationQuat() const { return _rotationQuat; }

void GameObject::setRotationQuat(glm::quat const& rotationQuat) {
  _rotationQuat = rotationQuat;
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
  result.rotationQuat = getRotationQuat();
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
        _resourceManager.get().getResource(_meshResourceRelativePath);

    if (meshResource.isResourceReady()) {
      meshReady = true;
    } else {
      meshResource.requestLoad();
    }
  }

  bool materialsReady = true;

  if (_materialRelativePaths.size()) {
    for (auto& matRelativePath : _materialRelativePaths) {
      runtime_resource::RuntimeResource& matResource =
          _resourceManager.get().getResource(matRelativePath);
      if (!matResource.isResourceReady()) {
        matResource.requestLoad();
        materialsReady = false;
      }
    }
  }

  if (meshReady && materialsReady) {
    rhi::DrawCall drawCall;
    for (auto const& materialResourcePath : _materialRelativePaths) {
      drawCall.materialIds.push_back(_resourceManager.get()
                                         .getResource(materialResourcePath)
                                         .getResourceId());
    }

    drawCall.meshId = _resourceManager.get()
                          .getResource(_meshResourceRelativePath)
                          .getResourceId();
    drawCall.transform = transform;
    _rhi.get().submitDrawCall(drawCall);
  }

  if (_directionalLight) {
    core::DirectionalLight directionalLight = *_directionalLight;
    directionalLight.direction = glm::normalize(
        transform * glm::vec4{0.0f, 0.0f, 1.0f, /*no translation:*/ 0.0f});
    _rhi.get().submitLight(directionalLight);
  }

  if (_spotlight) {
    core::Spotlight spotlight = *_spotlight;
    spotlight.position = getPosition();
    spotlight.direction = glm::normalize(
        transform * glm::vec4{0.0f, 0.0f, 1.0f, /*no translation*/ 0.0f});
    _rhi.get().submitLight(spotlight);
  }

  for (auto& child : getChildren()) {
    child.draw(transform);
  }
}

void GameObject::cleanup() {
  releaseMeshResource();
  releaseMaterialResources();
  removeEnvironmentMap();
  _objectId = invalidId;
}

void GameObject::updateTransform() {
  _transform = glm::mat4{1.0f};

  _transform *= glm::translate(_position);

  _transform *= glm::mat4_cast(_rotationQuat);

  _transform *= glm::scale(_scale);
}

void GameObject::releaseMaterialResources() {
  if (_materialRelativePaths.size()) {
    for (fs::path const& matRelativePath : _materialRelativePaths) {
      _resourceManager.get().getResource(matRelativePath).releaseRef();
    }
  }
  _materialRelativePaths.clear();
}

void GameObject::releaseMeshResource() {
  if (!_meshResourceRelativePath.empty()) {
    _resourceManager.get().getResource(_meshResourceRelativePath).releaseRef();
  }

  _meshResourceRelativePath.clear();
}
