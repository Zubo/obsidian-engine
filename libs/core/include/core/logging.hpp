#pragma once

#ifndef OBSIDIAN_LOGGING_VERBOSITY
#define OBSIDIAN_LOGGING_VERBOSITY 2
#endif

#if OBSIDIAN_LOGGING_VERBOSITY < 1

#define OBS_LOG_ERR(str)

#else

#include <iostream>

#define OBS_LOG_ERR(str) std::cout << "Error: " << str << std::endl;

#endif

#if OBSIDIAN_LOGGING_VERBOSITY < 2

#define OBS_LOG_WARN(str)

#else

#define OBS_LOG_WARN(str) std::cout << "Warning: " << str << std::endl;

#endif

#if OBSIDIAN_LOGGING_VERBOSITY < 3

#define OBS_LOG_MSG(str)

#else

#define OBS_LOG_MSG(str) std::cout << "Message: " << str << std::endl;

#endif
