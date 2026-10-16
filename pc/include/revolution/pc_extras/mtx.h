#pragma once
// PC extras: Wii SDK matrix declarations missing from Aurora's dolphin/mtx.h.
#include <dolphin/mtx.h>
#include <string.h>

typedef f32 Mtx23[2][3];
typedef f32 (*Mtx23P)[3];
typedef f32 Mtx33[3][3];
typedef f32 (*Mtx3P)[3];

#ifdef __cplusplus
extern "C" {
#endif

static inline void PSMTX44Identity(Mtx44 m) {
    memset(m, 0, sizeof(Mtx44));
    m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1.0f;
}
static inline void PSMTX44Copy(const Mtx44 src, Mtx44 dst) { memcpy(dst, src, sizeof(Mtx44)); }

#ifdef __cplusplus
}
#endif
