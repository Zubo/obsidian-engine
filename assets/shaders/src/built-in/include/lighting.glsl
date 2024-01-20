#ifndef _lighting_
#define _lighting_

#include "camera.glsl"

#define MAX_LIGHT_COUNT 8

layout(set = 1, binding = 2) uniform sampler2D shadowMap[MAX_LIGHT_COUNT];

struct DirectionalLight {
  mat4 viewProj;
  vec4 direction;
  vec4 color;
  vec4 intensity;
};

struct Spotlight {
  mat4 viewProj;
  vec4 direction;
  vec4 position;
  vec4 color;
  vec4 params; // x = intensity, y = cos of cutoff angle, z = cos of fadeout
               // angle
  vec4 attenuation; // x = linear attenuation, y = quadratic attenuation
};

layout(std140, set = 1, binding = 3) uniform LightCameraData {
  DirectionalLight directionalLights[MAX_LIGHT_COUNT];
  uint directionalLightShadowMapIndices[MAX_LIGHT_COUNT];
  Spotlight spotlights[MAX_LIGHT_COUNT];
  uint spotlightShadowMapIndices[MAX_LIGHT_COUNT];
  uint directionalLightCount;
  uint spotlightCount;
}
lights;

struct LightingResult {
  vec3 diffuse;
  vec3 specular;
};

float calculatePCF(uint shadowMapIdx, vec4 depthSpacePos, float bias) {
  float shadowMultiplier = 0.0f;

  const vec2 texelSize = 1.0f / textureSize(shadowMap[shadowMapIdx], 0);

  const int pcfCount = 2;
  for (int x = -pcfCount; x <= pcfCount; ++x) {
    for (int y = -pcfCount; y <= pcfCount; ++y) {
      const vec2 shadowUV =
          (depthSpacePos.xy / depthSpacePos.w + vec2(1.0f, 1.0f)) / 2.0f;
      const float shadowmapDepth =
          texture(shadowMap[shadowMapIdx], shadowUV + vec2(x, y) * texelSize.r)
              .r;

      if (depthSpacePos.z / depthSpacePos.w > shadowmapDepth + bias) {
        shadowMultiplier += 0.1f;
      } else {
        shadowMultiplier += 1.0f;
      }
    }
  }

  shadowMultiplier /= ((2 * pcfCount + 1) * (2 * pcfCount + 1));

  return shadowMultiplier;
}

#endif
