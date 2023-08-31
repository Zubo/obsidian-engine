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

layout(std140, set = 1, binding = 1) uniform LightCameraData {
  DirectionalLight directionalLights[MAX_LIGHT_COUNT];
  uint directionalLightCount;
}
lights;

layout(set = 2, binding = 0) uniform sampler2D albedoTex;

struct LightingResult {
  vec3 diffuse;
  vec3 specular;
};

LightingResult calculateDirectionalLighting() {

  LightingResult result = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

  for (int lightIdx = 0; lightIdx < lights.directionalLightCount; ++lightIdx) {
    vec4 intensity = lights.directionalLights[lightIdx].intensity;
    float diffuseIntensity =
        clamp(dot(normalize(-lights.directionalLights[lightIdx].direction.xyz),
                  normalize(inNormals)),
              0.0f, 1.0f);
    mat4 inverseView = inverse(cameraData.view);
    vec3 fragToCameraDirection = normalize(
        vec3(inverseView[3][0], inverseView[3][1], inverseView[3][2]) -
        inWorldPos);
    vec3 reflectedSunDirection = normalize(
        reflect(lights.directionalLights[lightIdx].direction.xyz, inNormals));

    float specularIntensity =
        pow(clamp(dot(fragToCameraDirection, reflectedSunDirection), 0.0f, 1.0),
            32);

    vec4 depthSpacePos =
        lights.directionalLights[lightIdx].viewProj * vec4(inWorldPos, 1.0f);

    float shadowMultiplier = 0.0f;
    float bias = max(
        0.005f,
        0.005 *
            (1.0 -
             dot(normalize(lights.directionalLights[lightIdx].direction.xyz),
                 normalize(inNormals))));

    vec2 texelSize = 1.0f / textureSize(shadowMap[lightIdx], 0);
    const int pcfCount = 2;
    for (int x = -pcfCount; x <= pcfCount; ++x) {
      for (int y = -pcfCount; y <= pcfCount; ++y) {
        vec2 shadowUV = (depthSpacePos.xy + vec2(1.0f, 1.0f)) / 2.0f;
        float shadowMapValue =
            texture(shadowMap[lightIdx], shadowUV + vec2(x, y) * texelSize.r).r;

        if (depthSpacePos.z > shadowMapValue + bias) {
          shadowMultiplier += 0.1f;
        } else {
          shadowMultiplier += 1.0f;
        }
      }
    }

    shadowMultiplier /= ((2 * pcfCount + 1) * (2 * pcfCount + 1));

    if (shadowMultiplier < 0.0001f) {
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

  vec3 finalColor = sampledColor * (directionalLightResult.diffuse +
                                    directionalLightResult.specular +
                                    sceneData.ambientColor.xyz);

  outFragColor = vec4(finalColor, 1.0f);
}
