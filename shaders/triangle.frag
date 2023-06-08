#version 450

layout(location = 0) in vec3 inColor;
layout(location = 0) out vec4 outFragColor;

layout(set = 0, binding = 1) uniform SceneData {
  vec4 fogColor;
  vec4 fogDistance;
  vec4 ambientColor;
  vec4 sunlightDirection;
  vec4 sunlgihtColor;
}
sceneData;

void main() { outFragColor = vec4(inColor, 1.0f) * sceneData.ambientColor; }