#version 460

layout(location = 0) in vec4 inWorldPos;

layout(location = 0) out vec4 outFragColor;

#include "include/pbr-material.glsl"

layout(set = 2, binding = 1) uniform sampler2D albedoTex;
layout(set = 2, binding = 2) uniform sampler2D normalTex;
layout(set = 2, binding = 3) uniform sampler2D metalnessTex;
layout(set = 2, binding = 4) uniform sampler2D roughnessTex;

void main() { outFragColor = vec4(1.0f, 0.1f, 0.2f, 1.0f); }
