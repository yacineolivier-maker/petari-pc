#pragma once

#include <revolution.h>
#include "nw4r/ut/resEndian.h"

namespace nw4r {
    namespace ut {
        typedef u32 SigWord;

        // File structs: big-endian in the file (BE() is the plain type on the Wii).
        struct BinaryFileHeader {
            BE(SigWord) signature;
            BE(u16) byteOrder;
            BE(u16) version;
            BE(u32) fileSize;
            BE(u16) headerSize;
            BE(u16) dataBlocks;
        };

        struct BinaryBlockHeader {
            BE(SigWord) kind;
            BE(u32) size;
        };

#if TARGET_PC
        static_assert(sizeof(BinaryFileHeader) == 0x10);
        static_assert(sizeof(BinaryBlockHeader) == 0x8);
#endif

        bool IsValidBinaryFile(const BinaryFileHeader *, u32, u16, u16);
    };
};
