#version 460
#extension GL_GOOGLE_include_directive : enable

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

#include "include/camera.glsl"

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
