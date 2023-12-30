#ifndef _env_maps_
#define _env_maps_

const uint maxEnvMaps = 64;

struct EnviromentMapData {
  vec3 pos;
  float radius;
};

layout(std140, set = 0, binding = 1) uniform EnvMapDataCollection {
  EnviromentMapData mapData[maxEnvMaps];
  uint count;
}
environmentMapData;

layout(set = 0, binding = 2) uniform samplerCube envMaps[maxEnvMaps];

int getNearestEnvMapInRadiusInd(vec3 fragWorldPos) {
  float ind = -1;
  float minDist = 1.0f / 0.0f;

  for (int i = 0; i < environmentMapData.count; i++) {
    float mapDist = distance(environmentMapData.mapData[i].pos, fragWorldPos);
    const float insideRadiusStepTest =
        step(mapDist, environmentMapData.mapData[i].radius);
    const float minDistStepTest = step(mapDist, minDist);
    const float testPassed = insideRadiusStepTest * minDistStepTest;

    ind = testPassed * i + (1.0f - testPassed) * ind;
    minDist = testPassed * mapDist + (1.0f - testPassed) * minDist;
  }

  return int(ind);
}

vec3 getReflectedColor(vec3 fragWorldPos, vec3 cameraWorldPos, vec3 normal) {
  int nearestEnvMapInd = getNearestEnvMapInRadiusInd(fragWorldPos);

  if (nearestEnvMapInd >= 0) {
    vec3 reflectedDir = reflect(fragWorldPos - cameraWorldPos, normal);
    return texture(envMaps[nearestEnvMapInd], reflectedDir).xyz;
  }

  return vec3(0.0f, 0.0f, 0.0f);
}

#endif
