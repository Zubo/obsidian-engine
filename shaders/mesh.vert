#version 460

layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vColor;
layout(location = 3) in vec2 vUV;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outUV;

layout(set = 0, binding = 0) uniform CameraBuffer {
  mat4 view;
  mat4 proj;
  mat4 viewProj;
}
cameraData;

layout(push_constant) uniform constants { mat4 model; }
PushConstants;

void main() {
  mat4 transformMatrix = cameraData.viewProj * PushConstants.model;
  gl_Position = transformMatrix * vec4(vPosition, 1.0f);
  outColor = vColor;
  outUV = vUV;
}
