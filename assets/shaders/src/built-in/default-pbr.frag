#version 460
#extension GL_GOOGLE_include_directive : enable

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 3) in vec2 inUV;
layout(location = 4) in mat3 inTBN;

layout(location = 0) out vec4 outFragColor;

layout(set = 2, binding = 1) uniform sampler2D albedoTex;
layout(set = 2, binding = 2) uniform sampler2D normalMapTex;
layout(set = 2, binding = 3) uniform sampler2D metalnessTex;
layout(set = 2, binding = 4) uniform sampler2D roughnessTex;

#include "include/pbr-lighting.glsl"
#include "include/pbr-material.glsl"

void main() {
  const mat4 inverseView = inverse(cameraData.view);
  const vec3 cameraPos =
      vec3(inverseView[3][0], inverseView[3][1], inverseView[3][2]);
  const vec3 fragToCameraDirection =
      normalize(vec3(inverseView[3][0], inverseView[3][1], inverseView[3][2]) -
                inWorldPos);

  vec3 normal =
      normalize(inTBN * (texture(normalMapTex, inUV).rgb * 2.0f - 1.0f));

  vec3 finalColor = {0.0f, 0.0f, 0.0f};

  for (int lightIdx = 0; lightIdx < lights.directionalLightCount; ++lightIdx) {
    const vec3 lightDir =
        normalize(lights.directionalLights[lightIdx].direction.xyz);

    finalColor +=
        reflectanceEquation(normal, -lightDir, fragToCameraDirection) *
        directionalLightRadiance(lightIdx, normal);
  }

  for (int lightIdx = 0; lightIdx < lights.spotlightCount; ++lightIdx) {
    const vec3 fragToLightDirection =
        normalize(lights.spotlights[lightIdx].position.xyz - inWorldPos);
    finalColor += reflectanceEquation(normal, fragToLightDirection,
                                      fragToCameraDirection) *
                  spotlightRadiance(lightIdx, normal, cameraPos);
  }

  outFragColor = vec4(finalColor / (1.0f + finalColor), 1.0f);
}
