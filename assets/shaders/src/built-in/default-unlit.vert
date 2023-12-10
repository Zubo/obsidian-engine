#version 460

layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 vNormal;

#ifdef _HAS_COLOR
layout(location = 2) in vec3 vColor;
#endif

#ifdef _HAS_UV
layout(location = 3) in vec2 vUV;
#endif

#ifdef _HAS_COLOR
layout(location = 0) out vec3 outColor;
#endif

#ifdef _HAS_UV
layout(location = 1) out vec2 outUV;
#endif

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

#ifdef _HAS_COLOR
  outColor = vColor;
#endif

#ifdef _HAS_UV
  outUV = vUV;
#endif
}
