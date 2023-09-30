#version 450

layout(location = 0) out float outFragColor;

layout(binding = 1, set = 0) uniform sampler2D inputTexture;
layout(push_constant) uniform PushConstants { mat3x3 kernel; }
pushConstants;

void main() {
  const vec2 textureSize = textureSize(inputTexture, 0);
  const vec2 dFragCoord = dFdx(gl_FragCoord.xy);

  float accumulatedColor = 0.0f;

  for (int i = -1; i < 2; ++i) {
    for (int j = -1; j < 2; ++j) {
      const vec2 sampleUv = (gl_FragCoord.xy + vec2(i, j)) / textureSize;
      accumulatedColor +=
          texture(inputTexture, sampleUv).r * pushConstants.kernel[i][j];
    }
  }

  outFragColor = accumulatedColor;
}
