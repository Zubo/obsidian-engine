#ifndef _pbr_lighting_
#define _pbr_lighting_

#include "lighting.glsl"
#include "pbr-material.glsl"

vec3 spotlightRadiance(int lightIdx, vec3 normal, vec3 cameraPos) {
  const float d = length(inWorldPos - lights.spotlights[lightIdx].position.xyz);

  const float attenuation = 1 / (0.01f + d * d);

  const float intensity = attenuation * lights.spotlights[lightIdx].params.x;

  const vec4 depthSpacePos =
      lights.spotlights[lightIdx].viewProj * vec4(inWorldPos, 1.0f);

  const vec3 fragToCameraDirection = normalize(cameraPos - inWorldPos);

  const vec3 fragToLightDirection =
      normalize(lights.spotlights[lightIdx].position.xyz - inWorldPos);

  const vec3 halfwayVec =
      normalize(fragToCameraDirection + fragToLightDirection);

  const uint shadowMapIdx = lights.spotlightShadowMapIndices[lightIdx];

  const float bias =
      max(0.000025f,
          0.00005 *
              (1.0f - dot(normalize(lights.spotlights[lightIdx].direction.xyz),
                          normalize(normal))));

  const float shadowMultiplier =
      calculatePCF(shadowMapIdx, depthSpacePos, bias);

  const float cosAngle =
      dot(normalize(inWorldPos - lights.spotlights[lightIdx].position.xyz),
          normalize(lights.spotlights[lightIdx].direction.xyz));

  const float cosCutoffAngle = lights.spotlights[lightIdx].params.y;
  const float cosFadeoutAngle = lights.spotlights[lightIdx].params.z;

  const float cutoff = step(cosAngle, cosCutoffAngle);
  const float incidenceFactor =
      clamp(dot(normal, fragToLightDirection), 0.0f, 1.0f);

  const float fadeout =
      clamp((cosAngle - cosFadeoutAngle) / (cosCutoffAngle - cosFadeoutAngle),
            0.0f, 1.0f);

  const vec3 result = ((1.0f - cutoff) + cutoff * fadeout) * incidenceFactor *
                      shadowMultiplier * intensity *
                      lights.spotlights[lightIdx].color.xyz;

  return result;
}

vec3 directionalLightRadiance(int lightIdx, vec3 normal) {
  const float intensity = lights.directionalLights[lightIdx].intensity.x;

  const vec4 depthSpacePos =
      lights.directionalLights[lightIdx].viewProj * vec4(inWorldPos, 1.0f);

  const float bias = max(
      0.005f,
      0.005f * (1.0 -
                dot(normalize(lights.directionalLights[lightIdx].direction.xyz),
                    normalize(normal))));

  const uint shadowMapIdx = lights.directionalLightShadowMapIndices[lightIdx];

  const float shadowMultiplier =
      calculatePCF(shadowMapIdx, depthSpacePos, bias);

  const vec3 lightDirectionReverse =
      -lights.directionalLights[lightIdx].direction.xyz;

  const float incidenceFactor =
      clamp(dot(normal, lightDirectionReverse), 0.0f, 1.0f);

  const vec3 result = incidenceFactor * shadowMultiplier * intensity *
                      lights.directionalLights[lightIdx].color.xyz;

  return result;
}

float getMetalness() { return texture(metalnessTex, inUV).b; }

float getRoughness() {
  if (materialData.metalnessAndRoughnessSeparate) {
    return texture(roughnessTex, inUV).r;
  } else {
    return texture(metalnessTex, inUV).g;
  }
}

float normalDistributionFunction(float roughness, float nhCosTheta) {
  return roughness * roughness /
         (4.0f * (nhCosTheta * (roughness - 1.0f) + 1.0f));
}

float geometryShlickGGX(float k, float nvCosTheta) {
  return nvCosTheta / (nvCosTheta * (1.0f - k) + k);
}

float geometryFunction(float k, vec3 normal, vec3 fragToLightDir,
                       vec3 fragToCameraDir) {
  return geometryShlickGGX(k, max(dot(normal, fragToLightDir), 0.0f)) *
         geometryShlickGGX(k, max(dot(normal, fragToCameraDir), 0.0f));
}

vec3 fresnelShlick(vec3 F0, float nhCosTheta) {
  return F0 + (1.0f - F0) * pow(1.0f - nhCosTheta, 5.0f);
}

vec3 reflectanceEquation(vec3 normal, vec3 fragToLightDir,
                         vec3 fragToCameraDir) {
  const float metalness = getMetalness();
  const float roughness = getRoughness();

  const vec3 lightHalfway = normalize(normal + fragToLightDir);

  const float nhCosTheta = max(dot(normal, lightHalfway), 0.0f);
  const float nLightCosTheta = max(dot(normal, fragToLightDir), 0.0f);
  const float nCameraCosTheta = max(dot(normal, fragToCameraDir), 0.0f);
  const float k = (roughness + 1.0f) * (roughness + 1.0f) / 8.0f;

  const float D = normalDistributionFunction(roughness, nhCosTheta);
  const float G = geometryFunction(k, normal, fragToLightDir, fragToCameraDir);

  const vec3 albedo = texture(albedoTex, inUV).xyz;

  const vec3 F0 = mix(vec3(0.04f), albedo, metalness);

  const vec3 F = fresnelShlick(F0, nhCosTheta);

  const vec3 L =
      (1.0f - F) * albedo +
      (F * D * G) / (4.0f * nLightCosTheta * nCameraCosTheta + 0.0001);

  return L;
}

#endif
