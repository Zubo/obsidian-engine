#version 450
#extension GL_GOOGLE_include_directive : enable

layout(location = 0) in vec3 inPos;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out mat4 outModel;

#include "include/camera.glsl"

layout(push_constant) uniform Constants { mat4 model; }
pushConstants;

void main() {
  gl_Position = cameraData.viewProj * pushConstants.model * vec4(inPos, 1.0f);
  outWorldPos = (pushConstants.model * vec4(inPos, 1.0f)).xyz;
  outModel = pushConstants.model;
}
