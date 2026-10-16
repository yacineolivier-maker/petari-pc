// Wii memory arenas for the PC port.
//
// The Wii has two RAM areas: MEM1 (24 MB) and MEM2 (64 MB). Aurora emulates the GameCube's single
// arena (MEM1, sized by AuroraConfig::mem1Size); MEM1 maps onto it. MEM2 is a separate host
// allocation made here. Both are larger than on the Wii (64-bit pointers make many structures
// bigger).

#include <revolution/os.h>

#include <cstdio>
#include <cstdlib>

namespace {

constexpr size_t kMem1Size = 256u * 1024 * 1024;  // keep in sync with platform.cpp
constexpr size_t kMem2Size = 256u * 1024 * 1024;
u8* sMem2Start = nullptr;
u8* sMem2End = nullptr;
void* sMem2ArenaLo = nullptr;
void* sMem2ArenaHi = nullptr;

void InitMem2() {
    if (sMem2Start != nullptr) {
        return;
    }
    sMem2Start = static_cast<u8*>(std::calloc(1, kMem2Size));
    if (sMem2Start == nullptr) {
        std::fprintf(stderr, "[pc] could not allocate MEM2 (%zu MB)\n", kMem2Size >> 20);
        std::abort();
    }
    sMem2End = sMem2Start + kMem2Size;
    sMem2ArenaLo = sMem2Start;
    sMem2ArenaHi = sMem2End;
}

}  // namespace

extern "C" {

void* OSGetMEM1ArenaLo(void) { return OSGetArenaLo(); }
void* OSGetMEM1ArenaHi(void) { return OSGetArenaHi(); }
void OSSetMEM1ArenaLo(void* lo) { OSSetArenaLo(lo); }
void OSSetMEM1ArenaHi(void* hi) { OSSetArenaHi(hi); }

void* OSGetMEM2ArenaLo(void) {
    InitMem2();
    return sMem2ArenaLo;
}
void* OSGetMEM2ArenaHi(void) {
    InitMem2();
    return sMem2ArenaHi;
}
void OSSetMEM2ArenaLo(void* lo) {
    InitMem2();
    sMem2ArenaLo = lo;
}
void OSSetMEM2ArenaHi(void* hi) {
    InitMem2();
    sMem2ArenaHi = hi;
}

// True if p is inside the emulated Wii memory (MEM1 or MEM2), where all JKRHeaps live.
// Host allocations (operator new on PC) never are.
bool pc_is_wii_memory(const void* p) {
    // MEM1 is Aurora's emulated main memory: AuroraConfig::mem1Size bytes (pc/src/platform.cpp).
    const u8* mem1Start = static_cast<const u8*>(OSPhysicalToCached(0));
    const u8* b = static_cast<const u8*>(p);
    if (mem1Start != nullptr && b >= mem1Start && b < mem1Start + kMem1Size) {
        return true;
    }
    return sMem2Start != nullptr && b >= sMem2Start && b < sMem2End;
}

void* OSAllocFromMEM1ArenaLo(u32 size, u32 align) { return OSAllocFromArenaLo(size, align); }

void* OSAllocFromMEM2ArenaLo(u32 size, u32 align) {
    InitMem2();
    uintptr_t p = (reinterpret_cast<uintptr_t>(sMem2ArenaLo) + align - 1) & ~(uintptr_t)(align - 1);
    uintptr_t end = (p + size + align - 1) & ~(uintptr_t)(align - 1);
    sMem2ArenaLo = reinterpret_cast<void*>(end);
    return reinterpret_cast<void*>(p);
}

}  // extern "C"
