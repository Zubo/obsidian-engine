#version 450

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormals;
layout(location = 3) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(set = 0, binding = 0) uniform CameraBuffer {
  mat4 view;
  mat4 proj;
  mat4 viewProj;
}
cameraData;

layout(set = 0, binding = 1) uniform SceneData { vec4 ambientColor; }
sceneData;

#define MAX_LIGHT_COUNT 8

layout(set = 1, binding = 0) uniform sampler2D shadowMap[MAX_LIGHT_COUNT];

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
  vec4 params; // x = intensity, y = cos of cutoff angle
};

layout(std140, set = 1, binding = 1) uniform LightCameraData {
  DirectionalLight directionalLights[MAX_LIGHT_COUNT];
  uint directionalLightShadowMapIndices[MAX_LIGHT_COUNT];
  Spotlight spotlights[MAX_LIGHT_COUNT];
  uint spotlightShadowMapIndices[MAX_LIGHT_COUNT];
  uint directionalLightCount;
  uint spotlightCount;
}
lights;

layout(set = 2, binding = 0) uniform sampler2D albedoTex;

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

LightingResult calculateSpotlights() {
  LightingResult result = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

  for (int lightIdx = 0; lightIdx < lights.spotlightCount; ++lightIdx) {
    const float intensity = lights.spotlights[lightIdx].params.x;

    const float cosCutoffAngle = lights.spotlights[lightIdx].params.y;
    const float cosFadeoutAngle = lights.spotlights[lightIdx].params.z;

    const float cosAngle =
        dot(normalize(inWorldPos - lights.spotlights[lightIdx].position.xyz),
            normalize(lights.spotlights[lightIdx].direction.xyz));

    const vec4 depthSpacePos =
        lights.spotlights[lightIdx].viewProj * vec4(inWorldPos, 1.0f);

    const mat4 inverseView = inverse(cameraData.view);
    const vec3 fragToCameraDirection = normalize(
        vec3(inverseView[3][0], inverseView[3][1], inverseView[3][2]) -
        inWorldPos);

    const vec3 reflectedLightDirection = normalize(
        reflect(lights.spotlights[lightIdx].direction.xyz, inNormals));

    const float specularIntensity = pow(
        clamp(dot(fragToCameraDirection, reflectedLightDirection), 0.0f, 1.0),
        32);

    const uint shadowMapIdx = lights.spotlightShadowMapIndices[lightIdx];

    const float bias =
        max(0.000025f,
            0.00005 * (1.0f -
                       dot(normalize(lights.spotlights[lightIdx].direction.xyz),
                           normalize(inNormals))));

    const float shadowMultiplier =
        calculatePCF(shadowMapIdx, depthSpacePos, bias);

    if (cosAngle > cosCutoffAngle) {
      result.diffuse +=
          shadowMultiplier * intensity * lights.spotlights[lightIdx].color.xyz;

      if (shadowMultiplier > 0.0001f) {
        result.specular += specularIntensity * intensity *
                           lights.spotlights[lightIdx].color.xyz;
      }
    } else {
      result.diffuse += shadowMultiplier * intensity *
                        clamp((cosAngle - cosFadeoutAngle) /
                                  (cosCutoffAngle - cosFadeoutAngle),
                              0.0f, 1.0f) *
                        lights.spotlights[lightIdx].color.xyz;
    }
  }

  return result;
}

LightingResult calculateDirectionalLighting() {

  LightingResult result = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

  for (int lightIdx = 0; lightIdx < lights.directionalLightCount; ++lightIdx) {
    vec3 intensity = lights.directionalLights[lightIdx].intensity.xyz;
    float diffuseIntensity =
        clamp(dot(normalize(-lights.directionalLights[lightIdx].direction.xyz),
                  normalize(inNormals)),
              0.0f, 1.0f);

    const mat4 inverseView = inverse(cameraData.view);
    const vec3 fragToCameraDirection = normalize(
        vec3(inverseView[3][0], inverseView[3][1], inverseView[3][2]) -
        inWorldPos);

    const vec3 reflectedLightDirection = normalize(
        reflect(lights.directionalLights[lightIdx].direction.xyz, inNormals));

    const float specularIntensity = pow(
        clamp(dot(fragToCameraDirection, reflectedLightDirection), 0.0f, 1.0),
        32);

    const vec4 depthSpacePos =
        lights.directionalLights[lightIdx].viewProj * vec4(inWorldPos, 1.0f);

    const float bias = max(
        0.005f,
        0.005 *
            (1.0 -
             dot(normalize(lights.directionalLights[lightIdx].direction.xyz),
                 normalize(inNormals))));

    const uint shadowMapIdx = lights.directionalLightShadowMapIndices[lightIdx];

    const float shadowMultiplier =
        calculatePCF(shadowMapIdx, depthSpacePos, bias);

    if (shadowMultiplier > 0.0001f) {
      result.specular += specularIntensity * intensity.xyz *
                         lights.directionalLights[lightIdx].color.xyz;
    }

    result.diffuse += shadowMultiplier * intensity.xyz * diffuseIntensity *
                      lights.directionalLights[lightIdx].color.xyz;
  }

  return result;
}

void main() {
  vec3 sampledColor = texture(albedoTex, inUV).xyz;

  LightingResult directionalLightResult = calculateDirectionalLighting();
  LightingResult spotlightResult = calculateSpotlights();

  vec3 finalColor =
      sampledColor *
      (spotlightResult.diffuse + directionalLightResult.diffuse +
       spotlightResult.specular + directionalLightResult.specular +
       sceneData.ambientColor.xyz);

  outFragColor = vec4(finalColor, 1.0f);
}
