#version 460

layout(location = 0) in vec3 vPosition;

layout(set = 0, binding = 0) uniform LightCameraBuffer {
  mat4 view;
  mat4 proj;
  mat4 viewProj;
}
lightCameraBuffer;

layout(push_constant) uniform constants { mat4 model; }
PushConstants;

void main() {
  mat4 transformMatrix = lightCameraBuffer.viewProj * PushConstants.model;

  gl_Position = transformMatrix * vec4(vPosition, 1.0f);
}
