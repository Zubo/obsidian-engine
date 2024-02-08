#ifndef _ssao_
#define _ssao_

#include "include/global-settings.glsl"

layout(set = 1, binding = 4) uniform sampler2D ssaoMap;

float getSsao() {
  const vec2 uv = gl_FragCoord.xy / vec2(globalSettings.swapchainWidth,
                                         globalSettings.swapchainHeight);
  return texture(ssaoMap, uv).r;
}

#endif
