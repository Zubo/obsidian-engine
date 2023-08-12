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

layout(set = 1, binding = 1) uniform sampler2D tex1;

void main() { outFragColor = vec4(texture(tex1, inUV).xyz, 1.0f); }