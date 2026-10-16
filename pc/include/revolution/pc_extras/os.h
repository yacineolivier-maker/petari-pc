#pragma once
// PC extras: Wii SDK OS declarations missing from Aurora's dolphin/os headers.
// Every revolution/os/* shim pulls in the whole Aurora OS API, like the Wii SDK headers do.
#include <dolphin/os.h>

// Wii SDK OS declarations (OSReset.h, OSResetSW.h, OSThread.h, OSArena) missing from Aurora.
#ifdef __cplusplus
extern "C" {
#endif
typedef void (*OSPowerCallback)(void);
OSPowerCallback OSSetPowerCallback(OSPowerCallback callback);
void OSRebootSystem(void);
void OSShutdownSystem(void);
void OSRestart(u32 resetCode);
void OSReturnToMenu(void);
void OSSleepTicks(OSTime ticks);
// MEM1/MEM2 arenas (implemented in pc/src/os/wii_arena.cpp).
void* OSGetMEM1ArenaLo(void);
void* OSGetMEM1ArenaHi(void);
void OSSetMEM1ArenaLo(void* lo);
void OSSetMEM1ArenaHi(void* hi);
void* OSGetMEM2ArenaLo(void);
void* OSGetMEM2ArenaHi(void);
void OSSetMEM2ArenaLo(void* lo);
void OSSetMEM2ArenaHi(void* hi);
void* OSAllocFromMEM1ArenaLo(u32 size, u32 align);
void* OSAllocFromMEM2ArenaLo(u32 size, u32 align);
#ifdef __cplusplus
}
#endif
