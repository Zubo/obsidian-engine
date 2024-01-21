#ifndef _blinn_phong_lighting
#define _blinn_phong_lighting

#include "lighting.glsl"
#include "lit-material.glsl"

// Light intensity is tuned for PBR, so we need to scale it for blinn phong
#define LIGHT_INTENSITY_FACTOR 0.3f

LightingResult calculateBlinnPhongSpotlights(vec3 normal) {
  const mat4 inverseView = inverse(cameraData.view);
  vec3 cameraPos =
      vec3(inverseView[3][0], inverseView[3][1], inverseView[3][2]);
  LightingResult result = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

  for (int lightIdx = 0; lightIdx < lights.spotlightCount; ++lightIdx) {
    const float d =
        length(inWorldPos - lights.spotlights[lightIdx].position.xyz);
    const float attenuation =
        1 / (1.0f + lights.spotlights[lightIdx].attenuation.x * d +
             lights.spotlights[lightIdx].attenuation.y * d * d);

    const float intensity = attenuation * lights.spotlights[lightIdx].params.x *
                            LIGHT_INTENSITY_FACTOR;

    const float cosCutoffAngle = lights.spotlights[lightIdx].params.y;
    const float cosFadeoutAngle = lights.spotlights[lightIdx].params.z;

    const float cosAngle =
        dot(normalize(inWorldPos - lights.spotlights[lightIdx].position.xyz),
            normalize(lights.spotlights[lightIdx].direction.xyz));

    const vec4 depthSpacePos =
        lights.spotlights[lightIdx].viewProj * vec4(inWorldPos, 1.0f);

    const vec3 fragToCameraDirection = normalize(cameraPos - inWorldPos);

    const vec3 fragToLightDirection =
        normalize(lights.spotlights[lightIdx].position.xyz - inWorldPos);

    const vec3 halfwayVec =
        normalize(fragToCameraDirection + fragToLightDirection);

    const float specularIntensity =
        pow(max(dot(normal, halfwayVec), 0.0f), materialData.shininess);

    const uint shadowMapIdx = lights.spotlightShadowMapIndices[lightIdx];

    const float bias =
        max(0.000025f,
            0.00005 * (1.0f -
                       dot(normalize(lights.spotlights[lightIdx].direction.xyz),
                           normalize(normal))));

    const float shadowMultiplier =
        calculatePCF(shadowMapIdx, depthSpacePos, bias);

    const float cutoff = step(cosAngle, cosCutoffAngle);

    result.diffuse += (1.0f - cutoff) * shadowMultiplier * intensity *
                      lights.spotlights[lightIdx].color.xyz;

    result.specular += (1.0f - cutoff) * step(0.5f, shadowMultiplier) *
                       shadowMultiplier * specularIntensity * intensity *
                       lights.spotlights[lightIdx].color.xyz;

    result.diffuse +=
        cutoff * shadowMultiplier * intensity *
        clamp((cosAngle - cosFadeoutAngle) / (cosCutoffAngle - cosFadeoutAngle),
              0.0f, 1.0f) *
        lights.spotlights[lightIdx].color.xyz;
  }

  return result;
}

LightingResult calculateBlinnPhongDirectionalLighting(vec3 normal) {
  const mat4 inverseView = inverse(cameraData.view);
  const vec3 fragToCameraDirection =
      normalize(vec3(inverseView[3][0], inverseView[3][1], inverseView[3][2]) -
                inWorldPos);

  LightingResult result = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

  for (int lightIdx = 0; lightIdx < lights.directionalLightCount; ++lightIdx) {
    vec3 intensity = lights.directionalLights[lightIdx].intensity.xyz *
                     LIGHT_INTENSITY_FACTOR;
    float diffuseIntensity =
        clamp(dot(normalize(-lights.directionalLights[lightIdx].direction.xyz),
                  normalize(normal)),
              0.0f, 1.0f);

    const vec3 reflectedLightDirection = normalize(
        reflect(lights.directionalLights[lightIdx].direction.xyz, normal));

    const vec3 halfwayVec =
        normalize(-lights.directionalLights[lightIdx].direction.xyz +
                  fragToCameraDirection);

    const float specularIntensity =
        pow(max(dot(normal, halfwayVec), 0.0f), materialData.shininess);

    const vec4 depthSpacePos =
        lights.directionalLights[lightIdx].viewProj * vec4(inWorldPos, 1.0f);

    const float bias = max(
        0.005f,
        0.005 *
            (1.0 -
             dot(normalize(lights.directionalLights[lightIdx].direction.xyz),
                 normalize(normal))));

    const uint shadowMapIdx = lights.directionalLightShadowMapIndices[lightIdx];

    const float shadowMultiplier =
        calculatePCF(shadowMapIdx, depthSpacePos, bias);

    result.specular += step(0.5f, shadowMultiplier) * shadowMultiplier *
                       specularIntensity * intensity.xyz *
                       lights.directionalLights[lightIdx].color.xyz;

    result.diffuse += shadowMultiplier * intensity.xyz * diffuseIntensity *
                      lights.directionalLights[lightIdx].color.xyz;
  }

  return result;
}

#endif
