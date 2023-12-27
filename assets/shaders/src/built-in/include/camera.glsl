#ifndef _camera_
#define _camera_

layout(set = 1, binding = 0) uniform CameraBuffer {
  mat4 view;
  mat4 proj;
  mat4 viewProj;
}
cameraData;

#endif
