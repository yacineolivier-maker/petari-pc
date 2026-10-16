#pragma once

#include "JSystem/JAudio2/JAISound.hpp"
#include "JSystem/JGeometry/TVec.hpp"

struct JAISeqDataUser;

class JAISoundHandles {
public:
    JAISoundHandles(JAISoundHandle* handles, int handleNum) {
        mHandles = handles;
        mNumHandles = handleNum;
    };

    JAISoundHandle& operator[](int n) {
        return mHandles[n];
    }

    JAISoundHandle* getHandle(int n) {
        return &mHandles[n];
    }

    JAISound* getSound(int n) {
        return getHandle(n)->getSound();
    }

    int getNumHandles() const {
        return mNumHandles;
    }

    JAISoundHandle* getHandleSoundID(JAISoundID);
    JAISoundHandle* getFreeHandle();
#if TARGET_PC
    JAISoundHandle* getHandleUserData(uintptr_t);
#else
    JAISoundHandle* getHandleUserData(u32);
#endif

    void setPos(const TVec3f&);

    /* 0x00 */ JAISoundHandle* mHandles;
    /* 0x04 */ int mNumHandles;
};
