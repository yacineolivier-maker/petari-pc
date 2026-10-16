#pragma once
// PC shim (manual): Aurora's dolphin/types.h, but with the SDK's original integer typedefs
// (s32/u32 = long/unsigned long). The decomp mixes u32 and "unsigned long" freely (declarations
// come from mangled names), so they must be the same type. On Windows long is 32 bits, so sizes
// and layouts are unchanged. Aurora's own sources are compiled with its int32_t typedefs; only
// extern "C" APIs cross that boundary, where both are 32-bit and ABI-compatible.
#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#endif

#ifdef TARGET_PC
#undef TARGET_PC
#include "../../../extern/aurora/include/dolphin/types.h"
#define TARGET_PC 1
#else
#include "../../../extern/aurora/include/dolphin/types.h"
#endif

typedef int BOOL;
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#undef NORETURN
#ifdef __cplusplus
#define NORETURN [[noreturn]]
#else
#define NORETURN
#endif
