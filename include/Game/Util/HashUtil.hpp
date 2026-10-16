#pragma once

#include <revolution/types.h>

class HashSortTable {
public:
    HashSortTable(u32);

#if TARGET_PC
    // Values are pointer-sized on PC: some users store pointers in the table (PlayerEvent,
    // EffectKeeper, MarioEffect). The u32* search overloads serve the users storing indices.
    typedef uintptr_t Value;

    bool add(const char*, Value, bool);
    bool add(u32, Value);
    bool addOrSkip(u32, Value);
    void sort();
    bool search(u32, Value*);
    bool search(const char*, Value*);
    bool search(const char*, const char*, Value*);
    bool search(u32, u32*);
    bool search(const char*, u32*);
    bool search(const char*, const char*, u32*);
    bool search(u32 a1, decltype(nullptr)) { return search(a1, (Value*)nullptr); }
    bool search(const char* a1, decltype(nullptr)) { return search(a1, (Value*)nullptr); }
    bool search(const char* a1, const char* a2, decltype(nullptr)) { return search(a1, a2, (Value*)nullptr); }
    void swap(const char*, const char*);

    /* 0x00 */ bool mHasBeenSorted;
    /* 0x04 */ u32* mHashCodes;
    /* 0x08 */ Value* _8;
    /* 0x0C */ u16* _C;
    /* 0x10 */ u16* _10;
    /* 0x14 */ u32 mCurrentLength;
    /* 0x18 */ u32 mMaxLength;
};
#else

    bool add(const char*, u32, bool);
    bool add(u32, u32);
    bool addOrSkip(u32, u32);
    void sort();
    bool search(u32, u32*);
    bool search(const char*, u32*);
    bool search(const char*, const char*, u32*);
    void swap(const char*, const char*);

    /* 0x00 */ bool mHasBeenSorted;
    /* 0x04 */ u32* mHashCodes;
    /* 0x08 */ u32* _8;
    /* 0x0C */ u16* _C;
    /* 0x10 */ u16* _10;
    /* 0x14 */ u32 mCurrentLength;
    /* 0x18 */ u32 mMaxLength;
};
#endif

namespace MR {
    u32 getHashCode(const char*);
    u32 getHashCodeLower(const char*);
};  // namespace MR
