#version 460

layout(location = 0) in vec3 vPosition;

layout(location = 0) out vec4 outWorldPos;

#include "include/camera.glsl"

layout(push_constant) uniform Constants { mat4 model; }
pushConstants;

void main() {
  mat4 transformMatrix = cameraData.viewProj * pushConstants.model;
  outWorldPos = pushConstants.model * vec4(vPosition, 1.0f);
  gl_Position = transformMatrix * vec4(vPosition, 1.0f);
}
