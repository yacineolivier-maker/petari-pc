#include "Game/Speaker/SpkWave.hpp"
#include <JSystem/JAudio2/JASCriticalSection.hpp>
#include <revolution/os.h>

SpkWave::SpkWave() : mResource(nullptr) {
}

#if TARGET_PC
namespace {
    u32 SwapWord(u32 v) {
        return (v >> 24) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | (v << 24);
    }

    // spkwave.csw is big-endian: [size, count, offsets[count]], then per wave WaveData (size in
    // bytes, loop start/end in samples, 16-bit samples). Swapped once to host order, in place.
    void SwapWaveResource(void* pResource) {
        u32* words = static_cast< u32* >(pResource);
        if (words[1] <= 0xFFFF) {
            return;  // already in host order
        }
        words[0] = SwapWord(words[0]);
        words[1] = SwapWord(words[1]);
        const u32 count = words[1];
        for (u32 i = 0; i < count; i++) {
            words[2 + i] = SwapWord(words[2 + i]);
            WaveData* wave = reinterpret_cast< WaveData* >(static_cast< u8* >(pResource) + words[2 + i]);
            wave->mSize = SwapWord(wave->mSize);
            wave->mLoopStartPos = SwapWord(wave->mLoopStartPos);
            wave->mLoopEndPos = SwapWord(wave->mLoopEndPos);
            u16* samples = reinterpret_cast< u16* >(wave->mWave);
            for (u32 j = 0; j < wave->mSize / 2; j++) {
                samples[j] = u16((samples[j] >> 8) | (samples[j] << 8));
            }
        }
    }
}  // namespace
#endif

void SpkWave::setResource(void* pResource) {
    JASCriticalSection crit;
#if TARGET_PC
    if (pResource != nullptr) {
        SwapWaveResource(pResource);
    }
#endif
    mResource = pResource;
}

s32 SpkWave::getWaveSize(s32 wave) const {
    if (mResource == nullptr) {
        return 0;
    }

    return getWaveData(wave)->mSize;
}

u32 SpkWave::getLoopStartPos(s32 wave) const {
    if (mResource == nullptr) {
        return 0;
    }

    return getWaveData(wave)->mLoopStartPos;
}

u32 SpkWave::getLoopEndPos(s32 wave) const {
    if (mResource == nullptr) {
        return 0;
    }

    return getWaveData(wave)->mLoopEndPos;
}

s16* SpkWave::getWave(s32 wave) const {
    if (mResource == nullptr) {
        return nullptr;
    }

    return getWaveData(wave)->mWave;
}

WaveData* SpkWave::getWaveData(s32 wave) const {
#if TARGET_PC
    // Host order (see SwapWaveResource).
    return (WaveData*)((u8*)mResource + *(u32*)((u8*)mResource + wave * 4 + 8));
#else
    return (WaveData*)((u32)mResource + *(u32*)((u32)mResource + wave * 4 + 8));
#endif
}
