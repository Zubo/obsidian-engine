#version 450

layout(location = 0) in vec3 inWorldPos;

#ifdef _HAS_COLOR
layout(location = 1) in vec3 inColor;
#endif

layout(location = 2) in vec3 inNormals;

#ifdef _HAS_UV
layout(location = 3) in vec2 inUV;
#endif

layout(location = 4) in mat3 inTBN;
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
layout(set = 1, binding = 2) uniform sampler2D ssaoMap;

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

layout(std140, set = 1, binding = 1) uniform LightCameraData {
  DirectionalLight directionalLights[MAX_LIGHT_COUNT];
  uint directionalLightShadowMapIndices[MAX_LIGHT_COUNT];
  Spotlight spotlights[MAX_LIGHT_COUNT];
  uint spotlightShadowMapIndices[MAX_LIGHT_COUNT];
  uint directionalLightCount;
  uint spotlightCount;
}
lights;

layout(std140, set = 2, binding = 0) uniform MaterialData {
  vec4 ambientColor;
  vec4 diffuseColor;
  vec4 specularColor;
  bool hasDiffuseTex;
  bool hasNormalMap;
  float shininess;
}
materialData;

layout(set = 2, binding = 1) uniform sampler2D diffuseTex;
layout(set = 2, binding = 2) uniform sampler2D normalMapTex;

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

LightingResult calculateSpotlights(vec3 normal) {
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

    const float intensity = attenuation * lights.spotlights[lightIdx].params.x;

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

    if (cosAngle > cosCutoffAngle) {
      result.diffuse +=
          shadowMultiplier * intensity * lights.spotlights[lightIdx].color.xyz;

      if (shadowMultiplier > 0.5f) {
        result.specular += shadowMultiplier * specularIntensity * intensity *
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

LightingResult calculateDirectionalLighting(vec3 normal) {
  const mat4 inverseView = inverse(cameraData.view);
  const vec3 fragToCameraDirection =
      normalize(vec3(inverseView[3][0], inverseView[3][1], inverseView[3][2]) -
                inWorldPos);

  LightingResult result = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

  for (int lightIdx = 0; lightIdx < lights.directionalLightCount; ++lightIdx) {
    vec3 intensity = lights.directionalLights[lightIdx].intensity.xyz;
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

    if (shadowMultiplier > 0.5f) {
      result.specular += shadowMultiplier * specularIntensity * intensity.xyz *
                         lights.directionalLights[lightIdx].color.xyz;
    }

    result.diffuse += shadowMultiplier * intensity.xyz * diffuseIntensity *
                      lights.directionalLights[lightIdx].color.xyz;
  }

  return result;
}

float getSsao() {
  const vec2 uv = gl_FragCoord.xy / textureSize(ssaoMap, 0);
  return texture(ssaoMap, uv).r;
}

void main() {
  vec3 normal = inNormals;

#ifdef _HAS_UV
  if (materialData.hasNormalMap) {
    normal = normalize(inTBN * (texture(normalMapTex, inUV).rgb * 2.0f - 1.0f));
  }
#endif

  LightingResult directionalLightResult = calculateDirectionalLighting(normal);
  LightingResult spotlightResult = calculateSpotlights(normal);

  const float ssao = getSsao();

  vec4 ambientColor =
      (ssao / 128.0f) * materialData.ambientColor * sceneData.ambientColor;

  vec4 diffuseColor = materialData.diffuseColor;

#ifdef _HAS_COLOR
  diffuseColor *= vec4(inColor, 1.0f);
#endif

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

  outFragColor =
      vec4((ambientColor + diffuseColor + specularColor).xyz, diffuseColor.w);
}
