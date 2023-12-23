#version 450

layout(location = 0) in vec3 inPos;

layout(location = 0) out vec3 outWorldPos;

#include "include/camera.glsl"

layout(push_constant) uniform Constants { mat4 model; }
pushConstants;

void main() {
  gl_Position = cameraData.viewProj * pushConstants.model * vec4(inPos, 1.0f);
  outWorldPos = (pushConstants.model * vec4(inPos, 1.0f)).xyz;
}
