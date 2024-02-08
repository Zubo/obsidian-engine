#ifndef _global_settings_
#define _global_settings_

layout(std140, set = 0, binding = 3) uniform GlobalSettings {
  uint swapchainWidth;
  uint swapchainHeight;
}
globalSettings;

#endif
