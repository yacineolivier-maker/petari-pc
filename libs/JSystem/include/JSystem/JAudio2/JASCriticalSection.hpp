#pragma once

#include <revolution/os/OSInterrupt.h>

class JASCriticalSection {
public:
#if TARGET_PC
    // OSDisableInterrupts is a no-op on PC: this is a recursive mutex shared with the audio
    // callback (pc/src/audio/pc_audio.cpp).
    JASCriticalSection();
    ~JASCriticalSection();
#else
    JASCriticalSection() : mInterruptState(OSDisableInterrupts()) {};
    ~JASCriticalSection() {
        OSRestoreInterrupts(mInterruptState);
    }
    /* 0x00 */ BOOL mInterruptState;
#endif
};
