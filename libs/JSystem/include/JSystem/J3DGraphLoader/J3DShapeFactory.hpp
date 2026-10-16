#pragma once

#include "JSystem/J3DGraphLoader/J3DModelLoader.hpp"

class J3DShape;
class J3DShapeMtx;
class J3DShapeDraw;
struct ResNTAB;

// SHP1 file data (big-endian, read in place).
struct J3DShapeInitData {
    /* 0x00 */ u8 mShapeMtxType;
    /* 0x02 */ BE(u16) mMtxGroupNum;
    /* 0x04 */ BE(u16) mVtxDescListIndex;
    /* 0x06 */ BE(u16) mMtxInitDataIndex;
    /* 0x08 */ BE(u16) mDrawInitDataIndex;
    /* 0x0C */ BE(f32) mRadius;
    /* 0x10 */ BE(Vec) mMin;
    /* 0x1C */ BE(Vec) mMax;
};

struct J3DShapeMtxInitData {
    /* 0x00 */ BE(u16) mUseMtxIndex;
    /* 0x02 */ BE(u16) mUseMtxCount;
    /* 0x04 */ BE(u32) mFirstUseMtxIndex;
};

struct J3DShapeDrawInitData {
    /* 0x00 */ BE(u32) mDisplayListSize;
    /* 0x04 */ BE(u32) mDisplayListIndex;
};

struct J3DShapeFactory {
    J3DShapeFactory(J3DShapeBlock const&);
    J3DShape* create(int, u32, GXVtxDescList*);
    J3DShapeMtx* newShapeMtx(u32, int, int) const;
    J3DShapeDraw* newShapeDraw(int, int) const;
    void allocVcdVatCmdBuffer(u32);
    s32 calcSize(int, u32);
    s32 calcSizeVcdVatCmdBuffer(u32);
    s32 calcSizeShapeMtx(u32, int, int) const;

    /* 0x00 */ J3DShapeInitData* mShapeInitData;
    /* 0x04 */ BE(u16)* mIndexTable;
    /* 0x08 */ GXVtxDescList* mVtxDescList;  // big-endian file data on PC (see getVtxDescList)
    /* 0x0C */ BE(u16)* mMtxTable;
    /* 0x10 */ u8* mDisplayListData;
    /* 0x14 */ J3DShapeMtxInitData* mMtxInitData;
    /* 0x18 */ J3DShapeDrawInitData* mDrawInitData;
    /* 0x1C */ u8* mVcdVatCmdBuffer;

    u32 getMtxGroupNum(int no) const {
        return mShapeInitData[mIndexTable[no]].mMtxGroupNum;
    }
    // On PC this points to the big-endian list in the file; J3DShapeFactory::create gives the shape
    // a host-endian copy.
    GXVtxDescList* getVtxDescList(int no) const {
        return (GXVtxDescList*)((u8*)mVtxDescList + mShapeInitData[mIndexTable[no]].mVtxDescListIndex);
    }
    f32 getRadius(int no) const {
        return mShapeInitData[mIndexTable[no]].mRadius;
    }
#if TARGET_PC
    Vec getMin(int no) const {
        return mShapeInitData[mIndexTable[no]].mMin;
    }
    Vec getMax(int no) const {
        return mShapeInitData[mIndexTable[no]].mMax;
    }
#else
    Vec& getMin(int no) const {
        return mShapeInitData[mIndexTable[no]].mMin;
    }
    Vec& getMax(int no) const {
        return mShapeInitData[mIndexTable[no]].mMax;
    }
#endif
};
