#include <obsidian/core/shapes.hpp>
#include <obsidian/core/utils/aabb.hpp>

#include <glm/glm.hpp>

#include <algorithm>
#include <limits>

using namespace obsidian::core;
using namespace obsidian::core::utils;

void obsidian::core::utils::updateAabb(glm::vec3 pos, Box3D& aabb) {
  if (pos.x > aabb.topCorner.x) {
    aabb.topCorner.x = pos.x;
  }
  if (pos.y > aabb.topCorner.y) {
    aabb.topCorner.y = pos.y;
  }
  if (pos.z > aabb.topCorner.z) {
    aabb.topCorner.z = pos.z;
  }

  if (pos.x < aabb.bottomCorner.x) {
    aabb.bottomCorner.x = pos.x;
  }
  if (pos.y < aabb.bottomCorner.y) {
    aabb.bottomCorner.y = pos.y;
  }
  if (pos.z < aabb.bottomCorner.z) {
    aabb.bottomCorner.z = pos.z;
  }
}

inline Box3D getNdcAabb(Box3D const aabb, glm::mat4 const mvpMat) {
  Box3D resultAabb = {-glm::vec3{std::numeric_limits<float>::infinity()},
                      glm::vec3{std::numeric_limits<float>::infinity()}};

  glm::vec3 const v[] = {aabb.topCorner, aabb.bottomCorner};

  for (std::size_t x = 0; x < 2; ++x) {
    for (std::size_t y = 0; y < 2; ++y) {
      for (std::size_t z = 0; z < 2; ++z) {
        glm::vec4 const projected =
            mvpMat * glm::vec4{v[x].x, v[y].y, v[z].z, 1.0f};
        // the division has to be with absolute value of w because it will flip
        // the box corners that are behind the camera causing bug
        glm::vec3 const normalized = projected / std::abs(projected.w);
        updateAabb(normalized, resultAabb);
      }
    }
  }

  return resultAabb;
}

bool obsidian::core::utils::isVisible(Box3D const& aabb,
                                      glm::mat4 const& mvpMat) {
  // check if projected aabb box overlaps with ndc box
  Box3D const deviceProjectedAabb = getNdcAabb(aabb, mvpMat);
  Box3D const deviceCoordCullingBox{{1.0f, 1.0f, 1.0f}, {-1.0f, -1.0f, 0.0f}};

  return core::overlaps(deviceProjectedAabb, deviceCoordCullingBox);
}
