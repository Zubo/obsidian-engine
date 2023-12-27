#version 460

layout(location = 0) in vec3 vPosition;

#include "include/camera.glsl"

layout(push_constant) uniform constants { mat4 model; }
PushConstants;

void main() {
  mat4 transformMatrix = cameraData.viewProj * PushConstants.model;

  gl_Position = transformMatrix * vec4(vPosition, 1.0f);
}
