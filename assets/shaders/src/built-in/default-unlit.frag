#version 450

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(std140, set = 2, binding = 0) uniform MaterialData {
  vec4 diffuseColor;
  bool hasDiffuseTex;
}
materialData;

layout(set = 2, binding = 1) uniform sampler2D diffuseTex;

void main() {
  outFragColor = materialData.diffuseColor;
  outFragColor *= outFragColor;

  if (materialData.hasDiffuseTex) {
    outFragColor *= texture(diffuseTex, inUV);
  }
}
