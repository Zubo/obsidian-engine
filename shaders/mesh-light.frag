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

layout(set = 2, binding = 0) uniform sampler2D shadowMap;

layout(set = 2, binding = 1) uniform CameraData {
  mat4 view;
  mat4 proj;
  mat4 viewProj;
}
lightCameraData;

layout(set = 1, binding = 1) uniform sampler2D tex1;

void main() {
  float lightIntensity = clamp(
      dot(normalize(-sceneData.sunlightDirection.xyz), normalize(inNormals)),
      0.0f, 1.0f);
  vec3 sampledColor = texture(tex1, inUV).xyz;

  vec4 depthSpacePos = lightCameraData.viewProj * vec4(inWorldPos, 1.0f);
  vec2 shadowUV = (depthSpacePos.xy + vec2(1.0f, 1.0f)) / 2.0f;
  float shadowMapValue = texture(shadowMap, shadowUV).r;

  float shadowMultiplier = 1.0f;

  if (depthSpacePos.z > shadowMapValue + 0.005f) {
    shadowMultiplier = 0.2f;
  }

  outFragColor = vec4(shadowMultiplier *
                          (lightIntensity * sceneData.sunlgihtColor.xyz +
                           sceneData.ambientColor.xyz) *
                          sampledColor,
                      1.0f);
}