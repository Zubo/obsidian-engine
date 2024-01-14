#ifndef _material_
#define _material_

layout(std140, set = 2, binding = 0) uniform MaterialData {
  vec4 ambientColor;
  vec4 diffuseColor;
  vec4 specularColor;
  bool hasDiffuseTex;
  bool hasNormalMap;
  bool reflection;
  float shininess;
}
materialData;

#endif
