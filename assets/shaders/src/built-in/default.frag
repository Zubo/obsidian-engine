#version 450

layout(location = 0) in vec3 inWorldPos;

#ifdef _HAS_COLOR
layout(location = 1) in vec3 inColor;
#endif

layout(location = 2) in vec3 inNormals;

#ifdef _HAS_UV
layout(location = 3) in vec2 inUV;
layout(location = 4) in mat3 inTBN;
#endif

layout(location = 0) out vec4 outFragColor;

#include "include/lighting.glsl"
#include "include/material.glsl"
#include "include/renderpass-data.glsl"
#include "include/ssao.glsl"

layout(set = 0, binding = 0) uniform SceneData { vec4 ambientColor; }
sceneData;

layout(set = 2, binding = 1) uniform sampler2D diffuseTex;
layout(set = 2, binding = 2) uniform sampler2D normalMapTex;

void main() {
  vec3 normal = inNormals;

#ifdef _HAS_UV
  if (materialData.hasNormalMap) {
    normal = normalize(inTBN * (texture(normalMapTex, inUV).rgb * 2.0f - 1.0f));
  }
#endif

  LightingResult directionalLightResult = calculateDirectionalLighting(normal);
  LightingResult spotlightResult = calculateSpotlights(normal);

  vec4 ambientColor = materialData.ambientColor * sceneData.ambientColor;

#ifdef _HAS_UV
  if (renderpassData.applySsao) {
    const float ssao = getSsao();
    ambientColor *= (ssao / 128.0f);
  }
#endif

  vec4 diffuseColor = materialData.diffuseColor;

  if (diffuseColor.w == 0.0) {
    discard;
  }

  diffuseColor *=
      vec4((spotlightResult.diffuse + directionalLightResult.diffuse), 1.0f);

  vec4 specularColor =
      materialData.specularColor *
      vec4((spotlightResult.specular + directionalLightResult.specular), 1.0f);

#ifdef _HAS_UV
  if (materialData.hasDiffuseTex) {
    vec4 diffuseTexSample = texture(diffuseTex, inUV);
    ambientColor *= diffuseTexSample;
    diffuseColor *= diffuseTexSample;
    specularColor *= diffuseTexSample;
  }
#endif

#ifdef _HAS_COLOR
  diffuseColor *= vec4(inColor, 1.0f);
#endif

  vec3 finalColor = (ambientColor + diffuseColor + specularColor).xyz;

  outFragColor = vec4(finalColor, diffuseColor.w);
}
