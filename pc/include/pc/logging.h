#pragma once
// Minimal logging for the PC port.
#include <cstdio>

#ifdef PC_STUB_LOGGING
#define STUB_LOG() std::fprintf(stderr, "[stub] %s\n", __FUNCTION__)
#else
#define STUB_LOG()
#endif
