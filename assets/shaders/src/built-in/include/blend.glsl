#ifndef _blend_
#define _blend_

vec3 blend(vec3 a, vec3 b) {
  vec3 result;

  result.r = step(0.5f, a.r) * 2.0f * a.r * b.r +
             (1 - step(0.5f, a.r)) * (1.0f - 2.0 * (1.0f - a.r) * (1.0f - b.r));

  result.g = step(0.5f, a.g) * 2.0f * a.g * b.g +
             (1 - step(0.5f, a.g)) * (1.0f - 2.0 * (1.0f - a.g) * (1.0f - b.g));

  result.b = step(0.5f, a.b) * 2.0f * a.b * b.b +
             (1 - step(0.5f, a.b)) * (1.0f - 2.0 * (1.0f - a.b) * (1.0f - b.b));

  return result;
}

#endif
