#version 450
#extension GL_GOOGLE_include_directive : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 3) in vec2 inUV;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out mat3x3 outTBN;

#include "include/camera.glsl"

layout(push_constant) uniform Constants { mat4 model; }
pushConstants;

layout(set = 1, binding = 3) uniform sampler2D noise;

void main() {
  gl_Position =
      cameraData.viewProj * pushConstants.model * vec4(inPosition, 1.0f);

  const vec3 sampledNoise = vec3(texture(noise, inUV * 800.0f).xy, 0.0f);

  const vec3 normal = normalize(
      (inverse(transpose(pushConstants.model)) * vec4(inNormal, 1.0f)).xyz);
  const vec3 tangent =
      normalize(sampledNoise - normal * dot(sampledNoise, normal));
  const vec3 bitangent = normalize(cross(normal, tangent));

  outTBN = mat3x3(tangent, bitangent, normal);

  outWorldPos = (pushConstants.model * vec4(inPosition, 1.0f)).xyz;
}
