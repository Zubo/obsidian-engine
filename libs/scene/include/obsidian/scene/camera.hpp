#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <obsidian/serialization/scene_data_serialization.hpp>

namespace obsidian::serialization {

struct CameraData;

} /*namespace obsidian::serialization*/

namespace obsidian::scene {

glm::vec3 forward(serialization::CameraData const& cameraData);
glm::vec3 right(serialization::CameraData const& cameraData);

} /*namespace obsidian::scene */
