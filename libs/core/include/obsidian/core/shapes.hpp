#pragma once

#include <glm/glm.hpp>

namespace obsidian::core {

struct Box3D {
  glm::vec3 topCorner;
  glm::vec3 bottomCorner;
};

inline bool isInside(glm::vec3 v, Box3D const& b) {
  return (v.x > b.bottomCorner.x && v.x < b.topCorner.x) &&
         (v.y > b.bottomCorner.y && v.y < b.topCorner.y) &&
         (v.z > b.bottomCorner.z && v.z < b.topCorner.z);
}

inline bool overlaps(Box3D b1, Box3D b2) {
  return (b1.bottomCorner.x <= b2.topCorner.x) &&
         (b1.topCorner.x >= b2.bottomCorner.x) &&
         (b1.bottomCorner.y <= b2.topCorner.y) &&
         (b1.topCorner.y >= b2.bottomCorner.y) &&
         (b1.bottomCorner.z <= b2.topCorner.z) &&
         (b1.topCorner.z >= b2.bottomCorner.z);
}

inline glm::vec3 getCenter(Box3D const& box) {
  return (box.topCorner + box.bottomCorner) / 2.0f;
}

} /*namespace obsidian::core */
