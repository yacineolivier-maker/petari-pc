#include "Game/Speaker/SpkTable.hpp"

SpkTable::SpkTable() {
    mInitialized = false;
    mResourceCount = 0;
    mParameters = nullptr;
    mNames = nullptr;
}

void SpkTable::setResource(void* pRes) {
    mInitialized = false;

    s32* cursor = (s32*)pRes;

    s32 resourceCount = *cursor++;
    s32 entryOff = *cursor++;
    s32 dataOffsetsStartOff = *cursor++;
    s32* pIsDataOffsetsInitialized = cursor;
    BOOL isDataOffsetsInitialized = *cursor++;

    mResourceCount = resourceCount;

#if TARGET_PC
    // The name table holds 32-bit offsets that can't be relocated in place into 64-bit pointers:
    // build a side table instead (owned by this object, mNames is always PC-allocated). The file is
    // left untouched, so its "initialized" flag is ignored.
    // TODO(pc): the resource is big-endian; values are read natively here until the endian layer exists.
    (void)pIsDataOffsetsInitialized;
    (void)isDataOffsetsInitialized;
    mParameters = (SpkParameters*)((u8*)pRes + entryOff);

    const u32* nameOffsets = (const u32*)((u8*)pRes + dataOffsetsStartOff);
    const char** names = new const char*[mResourceCount];
    for (s32 i = 0; i < mResourceCount; i++) {
        names[i] = (const char*)pRes + nameOffsets[i];
    }

    delete[] mNames;
    mNames = names;
    mInitialized = true;
#else
    SpkParameters* entryOffset = (SpkParameters*)((s32)pRes + entryOff);
    mParameters = entryOffset;
    const char** names = (const char**)((s32)pRes + dataOffsetsStartOff);
    if (!isDataOffsetsInitialized) {
        for (s32 i = 0; i < mResourceCount; i++) {
            names[i] += (s32)pRes;
        }
    }

    mNames = names;
    *pIsDataOffsetsInitialized = TRUE;
    mInitialized = true;
#endif
}
