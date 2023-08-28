#version 450

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormals;
layout(location = 3) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(set = 0, binding = 1) uniform SceneData {
  vec4 fogColor;
  vec4 fogDistance;
  vec4 ambientColor;
  vec4 sunlightDirection;
  vec4 sunlgihtColor;
}
sceneData;

layout(set = 1, binding = 0) uniform sampler2D shadowMap;

layout(set = 1, binding = 1) uniform LightCameraData {
  mat4 view;
  mat4 proj;
  mat4 viewProj;
}
lightCameraData;

layout(set = 2, binding = 0) uniform sampler2D albedoTex;

void main() {
  float lightIntensity = clamp(
      dot(normalize(-sceneData.sunlightDirection.xyz), normalize(inNormals)),
      0.0f, 1.0f);
  vec3 sampledColor = texture(albedoTex, inUV).xyz;

  vec4 depthSpacePos = lightCameraData.viewProj * vec4(inWorldPos, 1.0f);

  float shadowMultiplier = 0.0f;

  float bias =
      max(0.005f, 0.005 * (1.0 - dot(normalize(sceneData.sunlightDirection.xyz),
                                     normalize(inNormals))));

  vec2 texelSize = 1.0f / textureSize(shadowMap, 0);
  const int pcfCount = 2;
  for (int x = -pcfCount; x <= pcfCount; ++x) {
    for (int y = -pcfCount; y <= pcfCount; ++y) {
      vec2 shadowUV = (depthSpacePos.xy + vec2(1.0f, 1.0f)) / 2.0f;
      float shadowMapValue =
          texture(shadowMap, shadowUV + vec2(x, y) * texelSize.r).r;

      if (depthSpacePos.z > shadowMapValue + bias) {
        shadowMultiplier += 0.2f;
      } else {
        shadowMultiplier += 1.0f;
      }
    }
  }

  shadowMultiplier /= ((2 * pcfCount + 1) * (2 * pcfCount + 1));

  outFragColor = vec4(shadowMultiplier *
                          (lightIntensity * sceneData.sunlgihtColor.xyz +
                           sceneData.ambientColor.xyz) *
                          sampledColor,
                      1.0f);
}
