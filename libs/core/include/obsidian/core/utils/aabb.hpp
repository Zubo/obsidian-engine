#pragma once

#include <glm/fwd.hpp>

namespace obsidian::core {

struct Box3D;

} /*namespace obsidian::core */

namespace obsidian::core::utils {

bool isVisible(Box3D const& aabb, glm::mat4 const& mvpMat);

void updateAabb(glm::vec3 pos, core::Box3D& aabb);

} /*namespace obsidian::core::utils*/
