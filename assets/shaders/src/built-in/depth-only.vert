#version 460

layout(location = 0) in vec3 vPosition;

layout(set = 1, binding = 0) uniform CameraBuffer {
  mat4 view;
  mat4 proj;
  mat4 viewProj;
}
cameraBuffer;

layout(push_constant) uniform constants { mat4 model; }
PushConstants;

void main() {
  mat4 transformMatrix = cameraBuffer.viewProj * PushConstants.model;

  gl_Position = transformMatrix * vec4(vPosition, 1.0f);
}
