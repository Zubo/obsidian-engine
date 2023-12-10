#version 460

layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 vNormal;

#ifdef _HAS_COLOR
layout(location = 2) in vec3 vColor;
#endif

#ifdef _HAS_UV
layout(location = 3) in vec2 vUV;
#endif

layout(location = 4) in vec3 vTangent;

layout(location = 0) out vec3 outWorldPos;

#ifdef _HAS_COLOR
layout(location = 1) out vec3 outColor;
#endif

layout(location = 2) out vec3 outNormals;

#ifdef _HAS_UV
layout(location = 3) out vec2 outUV;
#endif

layout(location = 4) out mat3 outTBN;

layout(set = 0, binding = 0) uniform CameraBuffer {
  mat4 view;
  mat4 proj;
  mat4 viewProj;
}
cameraData;

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
#endif

  vec3 transformedTan = mat3(modelMat) * vTangent;
  vec3 bitangent = cross(outNormals, transformedTan);
  outTBN = mat3(transformedTan, bitangent, outNormals);
}
