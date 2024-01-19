#ifndef _pbr_material_
#define _pbr_material_

layout(std140, set = 2, binding = 0) uniform MaterialData {
  bool metalnessAndRoughnessSeparate;
}
materialData;

#endif
