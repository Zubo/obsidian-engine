#version 460

layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 vNormal;
layout(location = 3) in vec2 vUV;
layout(location = 4) in vec3 vTangent;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 3) out vec2 outUV;
layout(location = 4) out mat3 outTBN;

#include "include/camera.glsl"

layout(push_constant) uniform Constants { mat4 model; }
pushConstants;

void main() {
  mat4 transformMatrix = cameraData.viewProj * pushConstants.model;
  outWorldPos = (pushConstants.model * vec4(vPosition, 1.0f)).xyz;
  gl_Position = transformMatrix * vec4(vPosition, 1.0f);

  outNormal = mat3(transpose(inverse(pushConstants.model))) * vNormal;

  const vec3 transformedTan = mat3(pushConstants.model) * vTangent;
  const vec3 bitangent = cross(outNormal, transformedTan);

  outTBN = mat3(transformedTan, bitangent, outNormal);

  outUV = vUV;
}
