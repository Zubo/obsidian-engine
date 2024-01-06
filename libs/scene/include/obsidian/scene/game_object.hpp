#pragma once

#include <obsidian/core/light_types.hpp>
#include <obsidian/rhi/resource_rhi.hpp>
#include <obsidian/rhi/rhi.hpp>
#include <obsidian/runtime_resource/runtime_resource.hpp>
#include <obsidian/runtime_resource/runtime_resource_manager.hpp>
#include <obsidian/serialization/game_object_data_serialization.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <filesystem>
#include <list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace obsidian::scene {

class GameObject {
public:
  using GameObjectId = std::size_t;
  static constexpr std::size_t invalidId = (std::size_t)-1;

  GameObject(rhi::RHI& rhi,
             runtime_resource::RuntimeResourceManager& resourceManager);

  GameObject(GameObject&& other) noexcept = default;

  ~GameObject();

  std::string_view getName() const;
  void setName(std::string_view name);

  void
  setMaterials(std::vector<std::filesystem::path> const& materialRelativePaths);
  std::vector<std::filesystem::path> const& getMaterialRelativePaths();

  void setMesh(std::filesystem::path meshRelativePath);
  std::filesystem::path const& getMeshRelativePath();

  void setDirectionalLight(core::DirectionalLight const& directionalLight);
  std::optional<core::DirectionalLight> getDirectionalLight() const;
  void removeDirectionalLight();

  void setSpotlight(core::Spotlight const& spotlight);
  std::optional<core::Spotlight> getSpotlight() const;
  void removeSpotlight();

  void setEnvironmentMap(float radius = 1.0f);
  float getEnvironmentMapRadius() const;
  bool hasEnvironmentMap() const;
  void updateEnvironmentMap();
  void removeEnvironmentMap();

  GameObjectId getId() const;

  glm::vec3 const& getPosition() const;
  void setPosition(glm::vec3 const& pos);

  glm::vec3 getEuler() const;
  void setEuler(glm::vec3 const& euler);
  glm::quat const& getRotationQuat() const;
  void setRotationQuat(glm::quat const& quat);

  glm::vec3 const& getScale() const;
  void setScale(glm::vec3 const& scale);

  glm::mat4 const& getTransform() const;

  GameObject& createChild();

  GameObjectId getParentId();
  GameObject* getParent();

  void destroyChild(GameObjectId id);

  std::list<GameObject> const& getChildren() const;
  std::list<GameObject>& getChildren();

  serialization::GameObjectData getGameObjectData() const;

  void draw(glm::mat4 const& parentTransform);

private:
  void updateTransform();
  void releaseMaterialResources();
  void releaseMeshResource();
  void destroyEnvironmentMapRHI();

  std::string _name;
  std::vector<std::filesystem::path> _materialRelativePaths;
  std::filesystem::path _meshResourceRelativePath;
  std::optional<core::DirectionalLight> _directionalLight;
  std::optional<core::Spotlight> _spotlight;
  std::optional<float> _envMapRadius;
  rhi::ResourceIdRHI _envMapResourceId = rhi::rhiIdUninitialized;
  GameObject* _parent = nullptr;
  // TODO: use better data structure to store GameObjects
  std::list<GameObject> _children;
  GameObjectId _objectId;
  glm::vec3 _position = {};
  glm::quat _rotationQuat = {};
  glm::vec3 _scale = {1.0f, 1.0f, 1.0f};
  glm::mat4 _transform{1.0f};
  rhi::RHI& _rhi;
  runtime_resource::RuntimeResourceManager& _resourceManager;

  static GameObjectId _idCounter;
};

template <typename GameObjectCollection>
void forEachGameObjAndChildren(GameObjectCollection& gameObjects,
                               std::function<void(GameObject&)> f) {
  for (auto& obj : gameObjects) {
    f(obj);

    forEachGameObjAndChildren(obj.getChildren(), f);
  }
}

} /*namespace obsidian::scene*/
