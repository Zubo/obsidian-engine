#version 460

layout(location = 0) in vec3 vPosition;

layout(set = 0, binding = 0) uniform LightCameraBuffer {
  mat4 view;
  mat4 proj;
  mat4 viewProj;
}
lightCameraBuffer;

struct ObjectData {
  mat4 modelMat;
};

layout(std140, set = 1, binding = 0) buffer ObjectDataBuffer {
  ObjectData objectData[];
}
objectDataBuffer;

void main() {
  mat4 transformMatrix = lightCameraBuffer.viewProj *
                         objectDataBuffer.objectData[gl_BaseInstance].modelMat;

  gl_Position = transformMatrix * vec4(vPosition, 1.0f);
  // glm maps z to [-1, 1] but vulkan needs [0, 1]
  gl_Position.z = (gl_Position.z / 2.0f) + 0.5f;
}