#pragma once
// Force-included into every game translation unit of the PC build (see CMakeLists.txt).
// Recreates the MSL and MWCC extensions the decompiled code relies on, so the original sources
// compile on a host compiler with as few edits as possible.

#ifdef __cplusplus
#include <cctype>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#else
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#include <revolution/types.h>

// MSL's math.h defines M_PI (as a float).
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#if defined(_MSC_VER) && !defined(__clang__)
// MWCC/GCC extensions used by the decomp.
#define __attribute__(x)
#define __declspec_weak
#ifndef __typeof__
#define __typeof__ decltype
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

// PowerPC intrinsics. The hardware estimates are replaced by exact results.
static inline float __frsqrte(float x) { return 1.0f / sqrtf(x); }
static inline float __fres(float x) { return 1.0f / x; }
static inline double __fabs(double x) { return fabs(x); }
static inline float __fabsf(float x) { return fabsf(x); }
static inline float __fnabsf(float x) { return -fabsf(x); }
static inline double __fnabs(double x) { return -fabs(x); }
static inline s32 __abs(s32 x) { return x < 0 ? -x : x; }
static inline float __fsel(float a, float b, float c) { return a >= 0.0f ? b : c; }
static inline float __fsels(float a, float b, float c) { return a >= 0.0f ? b : c; }
static inline u32 __cntlzw(u32 x) {
#if defined(_MSC_VER) && !defined(__clang__)
    unsigned long idx;
    return _BitScanReverse(&idx, x) ? 31 - idx : 32;
#else
    return x ? __builtin_clz(x) : 32;
#endif
}
static inline u32 __rlwimi(u32 a, u32 b, u32 sh, u32 mb, u32 me) {
    u32 rot = (b << sh) | (sh ? (b >> (32 - sh)) : 0);
    u32 mask = (mb <= me) ? ((0xFFFFFFFFu >> mb) & (0xFFFFFFFFu << (31 - me)))
                          : ((0xFFFFFFFFu >> mb) | (0xFFFFFFFFu << (31 - me)));
    return (rot & mask) | (a & ~mask);
}
static inline void* __memcpy(void* dst, const void* src, size_t n) { return memcpy(dst, src, n); }
// Cache-line zeroing/flushing: no-ops (or plain memset) on PC.
static inline void __dcbz(void* base, int off) { memset((char*)base + off, 0, 32); }
static inline void __dcbf(void* base, int off) { (void)base; (void)off; }
static inline void __dcbst(void* base, int off) { (void)base; (void)off; }
static inline void __sync(void) {}

#ifdef __cplusplus
}
#endif

// MSL's name for va_list.
typedef va_list __va_list;

#ifdef __cplusplus
namespace std {
// MSL's va_list wrapper. Code does `std::__tag_va_List args; va_start(args.list, fmt);`.
struct __tag_va_List {
    va_list list;
};

// MSL provides the C99 float functions in namespace std.
using ::acosf;
using ::asinf;
using ::atan2f;
using ::atanf;
using ::ceilf;
using ::cosf;
using ::expf;
using ::fabsf;
using ::floorf;
using ::fmodf;
using ::logf;
using ::powf;
using ::sinf;
using ::sqrtf;
using ::tanf;
}  // namespace std
#endif
