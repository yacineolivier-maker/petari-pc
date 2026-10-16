#pragma once

#include <revolution/gx.h>
#include <revolution/types.h>

class Color8 {
public:
    Color8() : mColor(-1) {
    }

    Color8(GXColor color) {
        set(color);
    }

    Color8(u8 r, u8 g, u8 b, u8 a) {
        set(r, g, b, a);
    }

    Color8(u32 color) {
#if TARGET_PC
        // 0xRRGGBBAA, the big-endian layout of r, g, b, a (mColor itself is in host order on PC).
        set(u8(color >> 24), u8(color >> 16), u8(color >> 8), u8(color));
#else
        mColor = color;
#endif
    }

    inline operator GXColor() const {
        return mGXColor;
    }

    inline operator u32() const {
#if TARGET_PC
        return (u32(r) << 24) | (u32(g) << 16) | (u32(b) << 8) | u32(a);
#else
        return mColor;
#endif
    }

    inline const Color8& operator=(const GXColor& rOther) {
        mGXColor = rOther;
        return *this;
    }

    inline const Color8& operator=(const Color8& rOther) {
        r = rOther.r;
        g = rOther.g;
        b = rOther.b;
        a = rOther.a;
        return *this;
    }

    void set(u8 _r, u8 _g, u8 _b, u8 _a) {
        r = _r;
        g = _g;
        b = _b;
        a = _a;
    }

    void set(GXColor color) {
        GXColor* const pColor = &mGXColor;
        *pColor = color;
    }

    union {
        struct {
            u8 r;
            u8 g;
            u8 b;
            u8 a;
        };
        u32 mColor;
        GXColor mGXColor;
    };
};

class Color10 {
public:
    Color10() : r(0x3FF), g(0x3FF), b(0x3FF), a(0x3FF) {
    }

    operator GXColorS10() const {
        return mColor;
    }

    union {
        struct {
            u16 r;
            u16 g;
            u16 b;
            u16 a;
        };
        GXColorS10 mColor;
    };
};
