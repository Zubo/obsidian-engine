#ifndef _ssao_
#define _ssao_

layout(set = 1, binding = 3) uniform sampler2D ssaoMap;

float getSsao() {
  const vec2 uv = gl_FragCoord.xy / textureSize(ssaoMap, 0);
  return texture(ssaoMap, uv).r;
}

#endif
