#version 450

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in mat4 inModel;

layout(location = 0) out vec4 outFragColor;

#include "include/blinn-phong-lighting.glsl"
#include "include/camera.glsl"
#include "include/environment-maps.glsl"
#include "include/lit-material.glsl"
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

  LightingResult directionalLightResult =
      calculateBlinnPhongDirectionalLighting(normal);
  LightingResult spotlightResult = calculateBlinnPhongSpotlights(normal);

  const mat4 inverseView = inverse(cameraData.view);
  vec3 cameraWorldPos =
      vec3(inverseView[3][0], inverseView[3][1], inverseView[3][2]);

  vec3 reflectedColor = float(materialData.reflection) *
                        getReflectedColor(inWorldPos, cameraWorldPos, normal);

  directionalLightResult.specular += reflectedColor;
  spotlightResult.specular += reflectedColor;

  vec3 resultColor = (vec3(0.2f, 0.2f, 1.0f) * directionalLightResult.diffuse +
                      directionalLightResult.specular +
                      spotlightResult.diffuse + spotlightResult.specular) /
                     4.0f;

  outFragColor = vec4(resultColor, 0.4f);
}
