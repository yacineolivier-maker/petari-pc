#pragma once
// PC extras: Wii SDK GX declarations missing from Aurora's dolphin/gx headers.
#include <dolphin/gx.h>

#ifdef __cplusplus
#include <cstring>

// The game and JSystem write GX commands straight to the write-gather pipe (GXWGFifo.u8 = x).
// On PC each member assignment is forwarded to Aurora's command FIFO instead.
namespace pc_gx {
struct WGPipe {
    struct U8 { void operator=(::u8 v) const { GXCmd1u8(v); } } u8;
    struct S8 { void operator=(::s8 v) const { GXCmd1u8((::u8)v); } } s8;
    struct U16 { void operator=(::u16 v) const { GXCmd1u16(v); } } u16;
    struct S16 { void operator=(::s16 v) const { GXCmd1u16((::u16)v); } } s16;
    struct U32 { void operator=(::u32 v) const { GXCmd1u32(v); } } u32;
    struct S32 { void operator=(::s32 v) const { GXCmd1u32((::u32)v); } } s32;
    struct F32 {
        void operator=(::f32 v) const {
            ::u32 bits;
            std::memcpy(&bits, &v, sizeof(bits));
            GXCmd1u32(bits);
        }
    } f32;
};
inline const WGPipe wgpipe{};
}  // namespace pc_gx

#undef GXWGFifo
#define GXWGFifo (::pc_gx::wgpipe)
#endif

#ifdef __cplusplus
extern "C" {
#endif
// Implemented in Aurora (GXLighting.cpp) but not declared in its headers.
void GXInitSpecularDirHA(GXLightObj* lt_obj, f32 nx, f32 ny, f32 nz, f32 hx, f32 hy, f32 hz);
#ifdef __cplusplus
}
#endif

// Wii SDK inline (GXVert.h): writes a raw f32 to the command FIFO.
static inline void GXCmd1f32(f32 v) {
    u32 bits;
    memcpy(&bits, &v, sizeof(bits));
    GXCmd1u32(bits);
}

#ifdef __cplusplus
extern "C" {
#endif
// Aurora has no GXSetZScaleOffset. pc/src/sdk_stubs.cpp keeps the scale/offset like the Wii's GX
// state and applies it to the viewport depth range, also on later GXSetViewport calls (the game's
// GXSetViewport is redirected there).
void GXSetZScaleOffset(f32 scale, f32 offset);
void pc_GXSetViewport(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz);
#ifdef __cplusplus
}
#endif
#define GXSetViewport pc_GXSetViewport

#ifdef __cplusplus
extern "C" {
#endif
// Implemented in pc/src/os/os_stubs.cpp (runs the draw-sync callback synchronously).
void GXSetDrawSync(u16 token);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
// Wii SDK GX functions missing from Aurora's headers.
// Need PC implementations (Aurora's GXGet.cpp has them as TODO): read back GXTexObj fields.
void GXGetTexObjAll(const GXTexObj* obj, void** image_ptr, u16* width, u16* height, GXTexFmt* format,
                    GXTexWrapMode* wrap_s, GXTexWrapMode* wrap_t, GXBool* mipmap);
void GXGetTexObjLODAll(const GXTexObj* obj, GXTexFilter* min_filt, GXTexFilter* mag_filt, f32* min_lod,
                       f32* max_lod, f32* lod_bias, GXBool* bias_clamp, GXBool* do_edge_lod,
                       GXAnisotropy* max_aniso);
#ifdef __cplusplus
}
#endif
