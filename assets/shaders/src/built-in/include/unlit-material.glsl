#ifndef _unlit_material_
#define _unlit_material_

layout(std140, set = 2, binding = 0) uniform MaterialData {
  vec4 color;
  bool hasColorTex;
}
materialData;

#endif
