#pragma once
// PC extras.
#include <dolphin/gd.h>

// Wii SDK GD inlines missing from Aurora (revolution/gd/GDBase.h, GDGeometry.h).
#ifndef PC_EXTRAS_GD_VTX_HELPERS
#define PC_EXTRAS_GD_VTX_HELPERS
static inline void GDTexCoord2f32(f32 x, f32 y) {
    GDWrite_f32(x);
    GDWrite_f32(y);
}
static inline void GDPosition3f32(f32 x, f32 y, f32 z) {
    GDWrite_f32(x);
    GDWrite_f32(y);
    GDWrite_f32(z);
}
static inline void GDColor4u8(u8 r, u8 g, u8 b, u8 a) {
    GDWrite_u8(r);
    GDWrite_u8(g);
    GDWrite_u8(b);
    GDWrite_u8(a);
}
static inline void GDBegin(GXPrimitive type, GXVtxFmt vtxfmt, u16 nverts) {
    GDWrite_u8((u8)(vtxfmt | type));
    GDWrite_u16(nverts);
}
static inline void GDEnd(void) {}
#endif
