#include <obsidian/scene/camera.hpp>
#include <obsidian/serialization/scene_data_serialization.hpp>

#include <glm/gtx/transform.hpp>

using namespace obsidian::scene;

glm::vec3 obsidian::scene::forward(
    obsidian::serialization::CameraData const& cameraData) {
  glm::vec3 constexpr worldX{1.f, 0.f, 0.0f};
  glm::vec3 constexpr worldY{0.f, 1.f, 0.f};
  glm::vec3 constexpr worldZ{0.f, 0.f, 1.f};

  glm::vec3 const cameraForward = glm::normalize(
      glm::rotate(cameraData.rotationRad.y, worldY) *
      glm::rotate(cameraData.rotationRad.x, worldX) * glm::vec4{-worldZ, 1.f});

  return cameraForward;
}

glm::vec3
obsidian::scene::right(obsidian::serialization::CameraData const& cameraData) {
  glm::vec3 constexpr worldY{0.f, 1.f, 0.f};

  glm::vec3 const cameraForward = forward(cameraData);
  glm::vec3 const cameraRight =
      glm::normalize(glm::cross(cameraForward, worldY));

  return cameraRight;
}
