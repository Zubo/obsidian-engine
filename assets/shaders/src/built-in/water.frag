#version 450

layout(location = 0) in vec3 inWorldPos;

layout(location = 0) out vec4 outFragColor;

#include "include/lighting.glsl"
#include "include/timer.glsl"

layout(set = 2, binding = 2) uniform sampler2D normalMapTex;

void main() {
  float offset = timer.miliseconds / 10000.0f;
  const vec3 normal =
      texture(normalMapTex, 0.1f * inWorldPos.xz + vec2(offset, 1.3f * offset))
          .xyz;

  LightingResult directionalLightResult = calculateDirectionalLighting(normal);
  LightingResult spotlightResult = calculateSpotlights(normal);

  outFragColor =
      vec4(0.05f, 0.05f, 1.0f, 1.0f) *
      vec4(directionalLightResult.diffuse + directionalLightResult.specular +
               spotlightResult.diffuse + spotlightResult.specular,
           0.8f);
}
