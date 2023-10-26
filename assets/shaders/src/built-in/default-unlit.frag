#version 450

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(set = 0, binding = 1) uniform SceneData {
  vec4 fogColor;
  vec4 fogDistance;
  vec4 ambientColor;
  vec4 sunlightDirection;
  vec4 sunlgihtColor;
}
sceneData;

layout(std140, set = 2, binding = 0) uniform MaterialData {
  vec4 diffuseColor;
  bool hasDiffuseTex;
}
materialData;

layout(set = 2, binding = 1) uniform sampler2D diffuseTex;

void main() {
  outFragColor = materialData.diffuseColor;

  if (materialData.hasDiffuseTex) {
    outFragColor *= vec4(texture(diffuseTex, inUV).xyz, 1.0f);
  }
}
