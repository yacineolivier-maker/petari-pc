#pragma once
// PC shim (manual): Aurora's types.h plus the additions from libs/RVL_SDK/include/revolution/types.h.
#include <dolphin/types.h>

#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

// Size parameter type of operator new/delete (see libs/RVL_SDK/include/revolution/types.h).
#define OPNEW_SIZE_T size_t

#ifndef __REGISTER
#define __REGISTER
#endif

#ifndef AT_ADDRESS
#define AT_ADDRESS(x)
#endif

#ifndef ALWAYS_INLINE
#define ALWAYS_INLINE
#endif
// NO_INLINE is placed after the declarator (`int f() const NO_INLINE {`), where MSVC's
// __declspec(noinline) is not allowed. It only matters for matching; drop it.
#undef NO_INLINE
#define NO_INLINE

// The decomp places ATTRIBUTE_ALIGN after the declarator (MWCC/GCC style), which MSVC's
// __declspec(align) can't do. Define it empty, like the original header does for non-MWCC.
#undef ATTRIBUTE_ALIGN
#define ATTRIBUTE_ALIGN(num)
#ifndef ATTRIBUTE_PACKED
#define ATTRIBUTE_PACKED
#endif
#ifndef ATTRIBUTE_WEAK
#define ATTRIBUTE_WEAK
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifndef ROUND_UP
// The masks are pointer-wide: with a u32 alignment, ~(N - 1) is a 32-bit mask that would clear the
// upper half of a 64-bit address.
#define ROUND_UP(x, align) (((x) + (align) - 1) & ~(uintptr_t)((align) - 1))
#endif
#ifndef ROUND_UP_PTR
#define ROUND_UP_PTR(x, align) ((void*)((((uintptr_t)(x)) + (align) - 1) & (~(uintptr_t)((align) - 1))))
#endif

#ifndef ALIGN_PREV
#define ALIGN_PREV(X, N) ((X) & ~(uintptr_t)((N) - 1))
#define ALIGN_NEXT(X, N) ALIGN_PREV(((X) + (N) - 1), N)
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(o) (s32)(sizeof(o) / sizeof(o[0]))
#endif
#ifndef ARRAY_SIZEU
#define ARRAY_SIZEU(o) (sizeof(o) / sizeof(o[0]))
#endif

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif
#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifndef FOURCC
#define FOURCC(c0, c1, c2, c3) (u32)((c0 & 0xFF) << 24 | (c1 & 0xFF) << 16 | (c2 & 0xFF) << 8 | (c3 & 0xFF))
#endif

#ifndef IS_ALIGNED
#define IS_ALIGNED(x, align) (((uintptr_t)(x) & ((align) - 1)) == 0)
#endif
#ifndef IS_NOT_ALIGNED
#define IS_NOT_ALIGNED(X, N) (((X) & ((N) - 1)) != 0)
#endif

// Comparing a reference to NULL is UB; the PC build always treats references as non-null.
#ifndef IS_REF_NULL
#define IS_REF_NULL(r) (0)
#define IS_REF_NONNULL(r) (1)
#endif
