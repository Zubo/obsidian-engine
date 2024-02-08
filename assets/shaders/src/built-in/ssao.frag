#version 450
#extension GL_GOOGLE_include_directive : enable

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in mat3x3 inTBN;

layout(location = 0) out float outFragColor;

#include "include/camera.glsl"

layout(std140, set = 1, binding = 2) uniform ssaoSamples { vec4 values[64]; }
SsaoSamples;

layout(set = 1, binding = 4) uniform sampler2D depth;

void main() {
  const float offsetRadius = 5.0f;
  const float maxDepthDiff = 0.001f;

  float occlusionFactor = 0.0f;

  for (int i = 0; i < 64; ++i) {
    const vec3 offsetTangentSpace = SsaoSamples.values[i].xyz;
    const vec3 offsetWorldSpace = inTBN * offsetTangentSpace;
    const vec3 sampleWorldPos = offsetRadius * offsetWorldSpace + inWorldPos;
    const vec4 samplePosClipSpace =
        cameraData.viewProj * vec4(sampleWorldPos, 1.0f);
    const vec3 samplePosNDC = samplePosClipSpace.xyz / samplePosClipSpace.w;
    const float depthMapValue =
        texture(depth, (samplePosNDC.xy * 0.5f + 0.5f)).r;

    const float rangeCheck = smoothstep(
        0.0f, 1.0f, offsetRadius / abs(depthMapValue - samplePosNDC.z));
    const float bias = 0.0005f;

    occlusionFactor += step(samplePosNDC.z - bias, depthMapValue);
  }

  outFragColor = occlusionFactor;
}
