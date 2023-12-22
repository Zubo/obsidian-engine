#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 3) in vec2 inUV;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUV;

#include "include/camera.glsl"

layout(push_constant) uniform Constants { mat4 model; }
pushConstants;

void main() {
  gl_Position =
      cameraData.viewProj * pushConstants.model * vec4(inPosition, 1.0f);

  outWorldPos = (pushConstants.model * vec4(inPosition, 1.0f)).xyz;
  outNormal =
      (inverse(transpose(pushConstants.model)) * vec4(inNormal, 1.0f)).xyz;
  outUV = inUV;
}
