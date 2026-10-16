#pragma once

// Big-endian resource field wrapper for nw4r file structs (BRFNT, BRLYT, BRLAN).
// The nw4r matching build has no "include" directory on its path, so helpers/endian.h is only
// pulled in on PC. On the Wii BE(T) is T: struct layout and codegen are unchanged.
#if TARGET_PC
#include "helpers/endian.h"
#else
#ifndef BE
#define BE(T) T
#endif
#endif
