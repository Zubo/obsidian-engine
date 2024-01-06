#include <obsidian/renderdoc/renderdoc.hpp>

#ifdef RENDERDOC_ENABLED

#include <cassert>
#include <cstddef>
#include <dlfcn.h>

#include <renderdoc_app.h>

RENDERDOC_API_1_6_0* renderdocApi = nullptr;

void loadRenderdocLibrary() {
  pRENDERDOC_GetAPI RENDERDOC_GetAPI = nullptr;

  if (void* mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD)) {
    RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
  } else if (void* mod = dlopen("librenderdoc.so", RTLD_NOW)) {
    RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
  }

  int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0,
                             reinterpret_cast<void**>(&renderdocApi));
  assert(ret == 1);
}

void renderdoc::initRenderdoc() { loadRenderdocLibrary(); }

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

  if (renderdocApi->IsTargetControlConnected()) {
    renderdocApi->ShowReplayUI();
  } else {
    renderdocApi->LaunchReplayUI(1, "");
  }
}

#else

void renderdoc::initRenderdoc() {}
void renderdoc::deinitRenderdoc() {}
void renderdoc::beginCapture() {}
void renderdoc::endCapture() {}

#endif
