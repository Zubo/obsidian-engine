#version 450

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out float outFragColor;

layout(set = 0, binding = 0) uniform CameraBuffer {
  mat4 view;
  mat4 proj;
  mat4 viewProj;
}
cameraData;

layout(std140, set = 1, binding = 0) uniform ssaoSamples { vec4 values[64]; }
SsaoSamples;

layout(set = 1, binding = 1) uniform sampler2D noise;

layout(set = 1, binding = 2) uniform sampler2D depth;

void main() {
  const vec3 sampledNoise = vec3(texture(noise, inUV).xy, 0.0f);
  const vec3 normal = normalize(inNormal);
  const vec3 tangent = normalize(sampledNoise - dot(sampledNoise, normal));
  const vec3 bitangent = cross(normal, tangent);

  const mat3x3 TBN = mat3x3(tangent, bitangent, normal);

  const float currentDepth = gl_FragCoord.z / gl_FragCoord.w;

  float occlusionFactor = 0.0f;

  for (int i = 0; i < 64; ++i) {
    const vec3 offsetTangentSpace = SsaoSamples.values[i].xyz;
    const vec3 offsetWorldSpace = TBN * offsetTangentSpace;
    const vec3 sampleWorldPos = offsetWorldSpace + inWorldPos;
    const vec4 samplePosClipSpace =
        cameraData.viewProj * vec4(sampleWorldPos, 1.0f);
    const vec3 samplePosNDC = samplePosClipSpace.xyz / samplePosClipSpace.w;

    if (samplePosNDC.z > currentDepth) {
      occlusionFactor += samplePosNDC.z - currentDepth;
    }
  }

  outFragColor = occlusionFactor;
}