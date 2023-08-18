#include <renderdoc/renderdoc.hpp>

#ifdef RENDERDOC_ENABLED

#include <cassert>
#include <cstddef>
#include <dlfcn.h>

#include <renderdoc_app.h>

RENDERDOC_API_1_6_0* renderdocApi = nullptr;

void loadRenderdocLibrary() {
  if (void* mod = dlopen(RENDERDOC_PATH, RTLD_NOW | RTLD_LOCAL)) {
    pRENDERDOC_GetAPI RENDERDOC_GetAPI =
        (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
    int ret =
        RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void**)&renderdocApi);
    assert(ret == 1);
  };
}

void renderdoc::initRenderdoc() {
  loadRenderdocLibrary();
  renderdocApi->LaunchReplayUI(1, "");
}

void renderdoc::deinitRenderdoc() {
  // RENDERDOC_API_1_6_0::RemoveHooks() is not implemented on linux.
}

void renderdoc::beginCapture() {
  assert(renderdocApi);
  renderdocApi->StartFrameCapture(NULL, NULL);
}

void renderdoc::endCapture() {
  assert(renderdocApi);
  renderdocApi->EndFrameCapture(NULL, NULL);
}

#else

void renderdoc::initRenderdoc() {}
void renderdoc::deinitRenderdoc() {}
void renderdoc::beginCapture() {}
void renderdoc::endCapture() {}

#endif
