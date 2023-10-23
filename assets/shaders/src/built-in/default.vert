#version 460

layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vColor;
layout(location = 3) in vec2 vUV;
layout(location = 4) in vec3 vTangent;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outColor;
layout(location = 2) out vec3 outNormals;
layout(location = 3) out vec2 outUV;
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
  outColor = vColor;
  outNormals = normalize((transpose(inverse(modelMat)) * vec4(vNormal, 1.0f)).xyz);
  outUV = vUV;

  vec3 bitangent = cross(outNormals, vTangent);
  outTBN = mat3(vTangent, bitangent, outNormals);
}
