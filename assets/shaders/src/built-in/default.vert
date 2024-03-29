#version 460
#extension GL_GOOGLE_include_directive : enable

layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 vNormal;

#ifdef _HAS_COLOR
layout(location = 2) in vec3 vColor;
#endif

#ifdef _HAS_UV
layout(location = 3) in vec2 vUV;
layout(location = 4) in vec3 vTangent;
#endif

layout(location = 0) out vec3 outWorldPos;

#ifdef _HAS_COLOR
layout(location = 1) out vec3 outColor;
#endif

layout(location = 2) out vec3 outNormals;

#ifdef _HAS_UV
layout(location = 3) out vec2 outUV;
layout(location = 4) out mat3 outTBN;
#endif

#include "include/camera.glsl"

layout(push_constant) uniform constants { mat4 model; }
PushConstants;

void main() {
  mat4 modelMat = PushConstants.model;
  mat4 transformMatrix = cameraData.viewProj * modelMat;
  vec4 pos = transformMatrix * vec4(vPosition, 1.0f);

  gl_Position = pos;

  outWorldPos = (modelMat * vec4(vPosition, 1.0)).xyz;

#ifdef _HAS_COLOR
  outColor = vColor;
#endif

  outNormals =
      normalize((transpose(inverse(modelMat)) * vec4(vNormal, 1.0f)).xyz);

#ifdef _HAS_UV
  outUV = vUV;
  vec3 transformedTan = mat3(modelMat) * vTangent;
  vec3 bitangent = cross(outNormals, transformedTan);
  outTBN = mat3(transformedTan, bitangent, outNormals);
#endif
}
