#version 460

layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vColor;
layout(location = 3) in vec2 vUV;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec3 outNormals;
layout(location = 2) out vec2 outUV;

layout(set = 0, binding = 0) uniform CameraBuffer {
  mat4 view;
  mat4 proj;
  mat4 viewProj;
}
cameraData;

struct ObjectData {
  mat4 modelMat;
};

layout(std140, set = 1, binding = 0) buffer ObjectDataBuffer {
  ObjectData objectData[];
}
objectDataBuffer;

void main() {
  mat4 modelMat = objectDataBuffer.objectData[gl_BaseInstance].modelMat;
  mat4 transformMatrix = cameraData.viewProj * modelMat;
  gl_Position = transformMatrix * vec4(vPosition, 1.0f);

  outColor = vColor;
  outNormals = (transpose(inverse(modelMat)) * vec4(vNormal, 1.0f)).xyz;
  outUV = vUV;
}
