#pragma once

#include <revolution/os.h>

class JKRArchive;

namespace JASResArcLoader {

#if TARGET_PC
    // The callback argument carries a pointer (e.g. JAUSeqDataBlockMgr passes a JSULink*).
    typedef uintptr_t LoadCallbackArg;
#else
    typedef u32 LoadCallbackArg;
#endif
    typedef void (*LoadCallback)(u32, LoadCallbackArg);

    struct TLoadResInfo {
        TLoadResInfo(JKRArchive* archive, u16 id, void* buffer, u32 size);

        /* 0x0 */ JKRArchive* mArchive;
        /* 0x4 */ u16 mResID;
        /* 0x8 */ void* mBuffer;
        /* 0xC */ u32 mBufferSize;
        /* 0x10 */ LoadCallback mCallback;
        /* 0x14 */ LoadCallbackArg mCallbackArg;
        /* 0x18 */ OSMessageQueue* mMsgQueue;
    };

    u32 getResSize(JKRArchive const*, u16);
    static void loadResourceCallback(void*);
    int loadResourceAsync(JKRArchive*, u16, u8*, u32, LoadCallback, LoadCallbackArg);
}  // namespace JASResArcLoader

enum ResArcMessage {
    RESARC_MESSAGE_ERROR = -1,
    RESARC_MESSAGE_SUCCESS = 0,
};
