#pragma once

#include "JSystem/J3DGraphBase/J3DMatBlock.hpp"
#include "JSystem/J3DGraphLoader/J3DModelLoader.hpp"
#include "JSystem/J3DGraphBase/J3DShape.hpp"
#include "helpers/endian.h"
#if TARGET_PC
#include "helpers/endian_gx.hpp"
#include <string.h>
#endif

struct J3DMaterialInitData {
    /* 0x000 */ u8 mMaterialMode;
    /* 0x001 */ u8 mCullModeIdx;
    /* 0x002 */ u8 mColorChanNumIdx;
    /* 0x003 */ u8 mTexGenNumIdx;
    /* 0x004 */ u8 mTevStageNumIdx;
    /* 0x005 */ u8 mZCompLocIdx;
    /* 0x006 */ u8 mZModeIdx;
    /* 0x007 */ u8 mDitherIdx;
    /* 0x008 */ BE(u16) mMatColorIdx[2];
    /* 0x00C */ BE(u16) mColorChanIdx[4];
    /* 0x014 */ BE(u16) mAmbColorIdx[2];
    /* 0x018 */ u8 field_0x018[0x10];
    /* 0x028 */ BE(u16) mTexCoordIdx[8];
    /* 0x038 */ u8 field_0x038[0x10];
    /* 0x048 */ BE(u16) mTexMtxIdx[8];
    /* 0x058 */ u8 field_0x058[0x2c];
    /* 0x084 */ BE(u16) mTexNoIdx[8];
    /* 0x094 */ BE(u16) mTevKColorIdx[4];
    /* 0x09C */ u8 mTevKColorSel[0x10];
    /* 0x0AC */ u8 mTevKAlphaSel[0x10];
    /* 0x0BC */ BE(u16) mTevOrderIdx[0x10];
    /* 0x0DC */ BE(u16) mTevColorIdx[4];
    /* 0x0E4 */ BE(u16) mTevStageIdx[0x10];
    /* 0x104 */ BE(u16) mTevSwapModeIdx[0x10];
    /* 0x124 */ BE(u16) mTevSwapModeTableIdx[4];
    /* 0x12C */ u8 field_0x12c[0x18];
    /* 0x144 */ BE(u16) mFogIdx;
    /* 0x146 */ BE(u16) mAlphaCompIdx;
    /* 0x148 */ BE(u16) mBlendIdx;
    /* 0x14A */ BE(u16) mNBTScaleIdx;
};  // size 0x14C

struct J3DIndInitData {
    /* 0x000 */ bool mEnabled;
    /* 0x001 */ u8 mIndTexStageNum;
    /* 0x002 */ u8 field_0x002[2];
    /* 0x004 */ J3DIndTexOrderInfo mIndTexOrderInfo[3];
    /* 0x010 */ u8 field_0x010[4];
    /* 0x014 */ J3DIndTexMtxInfo mIndTexMtxInfo[3];
    /* 0x068 */ J3DIndTexCoordScaleInfo mIndTexCoordScaleInfo[3];
    /* 0x074 */ u8 field_0x074[4];
    /* 0x078 */ J3DIndTevStageInfo mIndTevStageInfo[0x10];
};  // size 0x138

struct J3DPatchingInfo {
    /* 0x0 */ BE(u16) mMatColorOffset;
    /* 0x2 */ BE(u16) mColorChanOffset;
    /* 0x4 */ BE(u16) mTexMtxOffset;
    /* 0x6 */ BE(u16) mTexNoOffset;
    /* 0x8 */ BE(u16) mTevRegOffset;
    /* 0xA */ BE(u16) mFogOffset;
    /* 0xC */ u8 field_0xc[4];
};  // size 0x10

struct J3DDisplayListInit {
    /* 0x0 */ BE(u32) mOffset;
    /* 0x4 */ BE(u32) field_0x4;
};  // size 8

#if TARGET_PC
// MAT3/MAT2/MDL3 entries that hold multi-byte values are runtime structs too (J3DTexMtx embeds a
// J3DTexMtxInfo, J3DFog derives from J3DFogInfo...), so they keep host fields and the factories
// convert a copy of the big-endian file entry when they read it. The file data is never modified.
inline J3DTexMtxInfo J3DSwapTexMtxInfo(const J3DTexMtxInfo& i_info) {
    J3DTexMtxInfo info;
    memcpy(&info, &i_info, sizeof(J3DTexMtxInfo));
    be_swap(info.mCenter);
    be_swap(info.mSRT.mScaleX);
    be_swap(info.mSRT.mScaleY);
    be_swap(info.mSRT.mRotation);
    be_swap(info.mSRT.mTranslationX);
    be_swap(info.mSRT.mTranslationY);
    be_swap(info.mEffectMtx);
    return info;
}

inline J3DFogInfo J3DSwapFogInfo(const J3DFogInfo& i_info) {
    J3DFogInfo info;
    memcpy(&info, &i_info, sizeof(J3DFogInfo));
    be_swap(info.mCenter);
    be_swap(info.mStartZ);
    be_swap(info.mEndZ);
    be_swap(info.mNearZ);
    be_swap(info.mFarZ);
    for (int i = 0; i < 10; i++) {
        be_swap(info.mFogAdjTable.r[i]);
    }
    return info;
}

inline J3DNBTScaleInfo J3DSwapNBTScaleInfo(const J3DNBTScaleInfo& i_info) {
    J3DNBTScaleInfo info;
    memcpy(&info, &i_info, sizeof(J3DNBTScaleInfo));
    be_swap(info.mScale);
    return info;
}

inline J3DIndTexMtxInfo J3DSwapIndTexMtxInfo(const J3DIndTexMtxInfo& i_info) {
    J3DIndTexMtxInfo info;
    memcpy(&info, &i_info, sizeof(J3DIndTexMtxInfo));
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 3; j++) {
            be_swap(info.field_0x0[i][j]);
        }
    }
    return info;
}

inline J3DCurrentMtxInfo J3DSwapCurrentMtxInfo(const J3DCurrentMtxInfo& i_info) {
    J3DCurrentMtxInfo info;
    info.mMtxIdxRegA = be32(i_info.mMtxIdxRegA);
    info.mMtxIdxRegB = be32(i_info.mMtxIdxRegB);
    return info;
}

static_assert(sizeof(J3DTexMtxInfo) == 0x64);
static_assert(sizeof(J3DFogInfo) == 0x2C);
static_assert(sizeof(J3DNBTScaleInfo) == 0x10);
static_assert(sizeof(J3DIndTexMtxInfo) == 0x1C);
static_assert(sizeof(J3DCurrentMtxInfo) == 0x8);
static_assert(sizeof(J3DLightInfo) == 0x34);
static_assert(sizeof(J3DColorChanInfo) == 0x8);
static_assert(sizeof(J3DTexCoordInfo) == 0x4);
static_assert(sizeof(J3DTevOrderInfo) == 0x4);
static_assert(sizeof(J3DTevStageInfo) == 0x14);
static_assert(sizeof(J3DTevSwapModeInfo) == 0x4);
static_assert(sizeof(J3DTevSwapModeTableInfo) == 0x4);
static_assert(sizeof(J3DIndTevStageInfo) == 0xC);
static_assert(sizeof(J3DAlphaCompInfo) == 0x8);
static_assert(sizeof(J3DBlendInfo) == 0x4);
static_assert(sizeof(J3DZModeInfo) == 0x4);
#endif

struct J3DTexCoord2Info;
class J3DCurrentMtxInfo;
class J3DMaterial;

class J3DMaterialFactory {
public:
    enum MaterialType {
        MATERIAL_TYPE_NORMAL = 0,
        MATERIAL_TYPE_LOCKED = 1,
        MATERIAL_TYPE_PATCHED = 2,
    };

    J3DMaterialFactory(J3DMaterialDLBlock const&);
    J3DMaterialFactory(J3DMaterialBlock const&);
    u16 countUniqueMaterials();
    u32 countTexGens(int) const;
    u32 countStages(int) const;
    J3DMaterial* create(J3DMaterial*, MaterialType, int, u32) const;
    J3DMaterial* createNormalMaterial(J3DMaterial*, int, u32) const;
    J3DMaterial* createPatchedMaterial(J3DMaterial*, int, u32) const;
    void modifyPatchedCurrentMtx(J3DMaterial*, int) const;
    J3DMaterial* createLockedMaterial(J3DMaterial*, int, u32) const;
    u32 calcSize(J3DMaterial*, MaterialType, int, u32) const;
    u32 calcSizeNormalMaterial(J3DMaterial*, int, u32) const;
    u32 calcSizePatchedMaterial(J3DMaterial*, int, u32) const;
    u32 calcSizeLockedMaterial(J3DMaterial*, int, u32) const;
    J3DGXColor newMatColor(int, int) const;
    const u8 newColorChanNum(int) const;
    J3DColorChan newColorChan(int, int) const;
    J3DGXColor newAmbColor(int, int) const;
    u32 newTexGenNum(int) const;
    J3DTexCoord newTexCoord(int, int) const;
    J3DTexMtx* newTexMtx(int, int) const;
    u8 newCullMode(int) const;
    u16 newTexNo(int, int) const;
    J3DTevOrder newTevOrder(int, int) const;
    J3DGXColorS10 newTevColor(int, int) const;
    J3DGXColor newTevKColor(int, int) const;
    const u8 newTevStageNum(int) const;
    J3DTevStage newTevStage(int, int) const;
    J3DTevSwapModeTable newTevSwapModeTable(int, int) const;
    u8 newIndTexStageNum(int) const;
    J3DIndTexOrder newIndTexOrder(int, int) const;
    J3DIndTexMtx newIndTexMtx(int, int) const;
    J3DIndTevStage newIndTevStage(int, int) const;
    J3DIndTexCoordScale newIndTexCoordScale(int, int) const;
    J3DFog newFog(int) const;
    J3DAlphaComp newAlphaComp(int) const;
    J3DBlend newBlend(int) const;
    J3DZMode newZMode(int) const;
    const u8 newZCompLoc(int) const;
    const u8 newDither(int) const;
    J3DNBTScale newNBTScale(int) const;

    u16 getMaterialID(int idx) const {
        return mpMaterialID[idx];
    }
    u8 getMaterialMode(int idx) const {
        return mpMaterialInitData[mpMaterialID[idx]].mMaterialMode;
    }

    /* 0x00 */ u16 mMaterialNum;
    /* 0x04 */ J3DMaterialInitData* mpMaterialInitData;
    /* 0x08 */ BE(u16)* mpMaterialID;
    /* 0x0C */ J3DIndInitData* mpIndInitData;
    /* 0x10 */ GXColor* mpMatColor;
    /* 0x14 */ u8* mpColorChanNum;
    /* 0x18 */ J3DColorChanInfo* mpColorChanInfo;
    /* 0x1C */ GXColor* mpAmbColor;
    /* 0x20 */ J3DLightInfo* mpLightInfo;
    /* 0x24 */ u8* mpTexGenNum;
    /* 0x28 */ J3DTexCoordInfo* mpTexCoordInfo;
    /* 0x2C */ J3DTexCoord2Info* mpTexCoord2Info;
    /* 0x30 */ J3DTexMtxInfo* mpTexMtxInfo;
    /* 0x34 */ J3DTexMtxInfo* field_0x34;
    /* 0x38 */ BE(u16)* mpTexNo;
    /* 0x3C */ BE(GXCullMode)* mpCullMode;
    /* 0x40 */ J3DTevOrderInfo* mpTevOrderInfo;
    /* 0x44 */ BE(GXColorS10)* mpTevColor;
    /* 0x48 */ GXColor* mpTevKColor;
    /* 0x4C */ u8* mpTevStageNum;
    /* 0x50 */ J3DTevStageInfo* mpTevStageInfo;
    /* 0x54 */ J3DTevSwapModeInfo* mpTevSwapModeInfo;
    /* 0x58 */ J3DTevSwapModeTableInfo* mpTevSwapModeTableInfo;
    /* 0x5C */ J3DFogInfo* mpFogInfo;
    /* 0x60 */ J3DAlphaCompInfo* mpAlphaCompInfo;
    /* 0x64 */ J3DBlendInfo* mpBlendInfo;
    /* 0x68 */ J3DZModeInfo* mpZModeInfo;
    /* 0x6C */ u8* mpZCompLoc;
    /* 0x70 */ u8* mpDither;
    /* 0x74 */ J3DNBTScaleInfo* mpNBTScaleInfo;
    /* 0x78 */ J3DDisplayListInit* mpDisplayListInit;
    /* 0x7C */ J3DPatchingInfo* mpPatchingInfo;
    /* 0x80 */ J3DCurrentMtxInfo* mpCurrentMtxInfo;
    /* 0x84 */ u8* mpMaterialMode;
};
