#pragma once

#include <revolution.h>
#include "nw4r/ut/resEndian.h"

namespace nw4r {
    namespace ut {
#if TARGET_PC
        // BRFNT pointer fields hold 32-bit offsets from the start of the file that the Wii code
        // relocates in place into pointers (ResFont::Rebuild). 4 bytes cannot hold a 64-bit
        // pointer, so on PC the file is left untouched and the offset is resolved on every access
        // against the font resource base (ResFontBase::mResource). 0 means null.
        template < typename T >
        struct FontResPtr {
            BE(u32) offset;

            T* Get(const void* pBase) const {
                const u32 offs = offset;
                return offs != 0 ? reinterpret_cast< T* >(const_cast< u8* >(static_cast< const u8* >(pBase)) + offs) : nullptr;
            }
        };
#define NW4R_FONT_RES_PTR(T) FontResPtr< T >
#else
#define NW4R_FONT_RES_PTR(T) T*
#endif

        struct CharWidths {
            s8 left;
            u8 glyphWidth;
            s8 charWidth;
        };

        // File structs below are big-endian in the file (BE() is the plain type on the Wii).
        struct FontTextureGlyph {
            u8 cellWidth;
            u8 cellHeight;
            s8 baselinePos;
            u8 maxCharWidth;
            BE(u32) sheetSize;
            BE(u16) sheetNum;
            BE(u16) sheetFormat;
            BE(u16) sheetRow;
            BE(u16) sheetLine;
            BE(u16) sheetWidth;
            BE(u16) sheetHeight;
            NW4R_FONT_RES_PTR(u8) sheetImage;
        };

        struct FontWidth {
            BE(u16) indexBegin;
            BE(u16) indexEnd;
            NW4R_FONT_RES_PTR(FontWidth) pNext;
            CharWidths widthTable[];
        };

        struct FontCodeMap {
            BE(u16) ccodeBegin;
            BE(u16) ccodeEnd;
            BE(u16) mappingMethod;
            BE(u16) reserved;
            NW4R_FONT_RES_PTR(FontCodeMap) pNext;
            BE(u16) mapInfo[];
        };

        struct FontInformation {
            u8 fontType;
            s8 linefeed;
            BE(u16) alterCharIndex;
            CharWidths defaultWidth;
            u8 encoding;
            NW4R_FONT_RES_PTR(FontTextureGlyph) pGlyph;
            NW4R_FONT_RES_PTR(FontWidth) pWidth;
            NW4R_FONT_RES_PTR(FontCodeMap) pMap;
            u8 height;
            u8 width;
            u8 ascent;
            u8 padding_[1];
        };

#if TARGET_PC
        static_assert(sizeof(CharWidths) == 0x3);
        static_assert(sizeof(FontTextureGlyph) == 0x18);
        static_assert(sizeof(FontWidth) == 0x8);
        static_assert(sizeof(FontCodeMap) == 0xC);
        static_assert(sizeof(FontInformation) == 0x18);
#endif
    };  // namespace ut
};  // namespace nw4r
