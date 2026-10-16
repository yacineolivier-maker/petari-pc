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
