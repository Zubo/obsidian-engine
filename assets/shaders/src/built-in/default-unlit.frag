#version 450

#ifdef _HAS_COLOR
layout(location = 0) in vec3 inColor;
#endif

#ifdef _HAS_UV
layout(location = 1) in vec2 inUV;
#endif

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

#ifdef _HAS_COLOR
  outFragColor *= vec4(inColor, 1.0f);
#endif

#ifdef _HAS_UV
  if (materialData.hasDiffuseTex) {
    outFragColor *= texture(diffuseTex, inUV);
  }
#endif
}
