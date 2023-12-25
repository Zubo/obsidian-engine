#version 450

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in mat4 inModel;

layout(location = 0) out vec4 outFragColor;

#include "include/lighting.glsl"
#include "include/timer.glsl"

layout(set = 2, binding = 2) uniform sampler2D normalMapTex;

void main() {
  float offset = timer.miliseconds / 50000.0f;
  const mat4 normalTransform = transpose(inverse(inModel));

  const vec3 sampledNormal = normalize(
      texture(normalMapTex, 0.1f * inWorldPos.xz + vec2(offset, 1.3f * offset))
          .xzy);

  const vec3 normal =
      normalize((normalTransform * vec4(sampledNormal, 1.0f)).xyz);

  LightingResult directionalLightResult = calculateDirectionalLighting(normal);
  LightingResult spotlightResult = calculateSpotlights(normal);

  outFragColor =
      vec4(0.2f, 0.2f, 1.0f, 1.0f) *
      vec4(directionalLightResult.diffuse + directionalLightResult.specular +
               spotlightResult.diffuse + spotlightResult.specular,
           0.2f);
}
