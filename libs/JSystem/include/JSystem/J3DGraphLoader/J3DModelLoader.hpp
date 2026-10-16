#pragma once

#include <revolution.h>
#include "helpers/endian.h"

// File offsets inside BMD/BDL blocks. The Wii code turns them into pointers in place; on PC a
// 64-bit pointer does not fit, so they stay 32-bit big-endian offsets from the block start.
#if TARGET_PC
#define J3D_OFFSET_PTR BE(u32)
#else
#define J3D_OFFSET_PTR void*
#endif

class J3DModelData;
class J3DMaterialTable;
struct J3DModelHierarchy;

enum J3DModelLoaderFlagTypes {
    J3DMLF_None = 0x00000000,

    J3DMLF_MtxSoftImageCalc = 0x00000001,
    J3DMLF_MtxMayaCalc = 0x00000002,
    J3DMLF_MtxBasicCalc = 0x00000004,
    J3DMLF_04 = 0x00000008,
    J3DMLF_MtxTypeMask =
        J3DMLF_MtxSoftImageCalc | J3DMLF_MtxMayaCalc | J3DMLF_MtxBasicCalc | J3DMLF_04,  // 0 - 2 (0 = Basic, 1 = SoftImage, 2 = Maya)

    J3DMLF_UseImmediateMtx = 0x00000010,
    J3DMLF_UsePostTexMtx = 0x00000020,
    J3DMLF_07 = 0x00000040,
    J3DMLF_08 = 0x00000080,
    J3DMLF_NoMatrixTransform = 0x00000100,
    J3DMLF_10 = 0x00000200,
    J3DMLF_11 = 0x00000400,
    J3DMLF_12 = 0x00000800,
    J3DMLF_13 = 0x00001000,
    J3DMLF_DoBdlMaterialCalc = 0x00002000,
    J3DMLF_15 = 0x00004000,
    J3DMLF_16 = 0x00008000,
    J3DMLF_TevNumShift = 0x00010000,
    J3DMLF_18 = 0x00020000,
    J3DMLF_UseSingleSharedDL = 0x00040000,
    J3DMLF_20 = 0x00080000,
    J3DMLF_21 = 0x00100000,
    J3DMLF_UseUniqueMaterials = 0x00200000,
    J3DMLF_23 = 0x00400000,
    J3DMLF_24 = 0x00800000,
    J3DMLF_Material_UseIndirect = 0x01000000,
    J3DMLF_26 = 0x02000000,
    J3DMLF_27 = 0x04000000,
    J3DMLF_Material_TexGen_Block4 = 0x08000000,
    J3DMLF_Material_PE_Full = 0x10000000,
    J3DMLF_Material_PE_FogOff = 0x20000000,
    J3DMLF_Material_Color_LightOn = 0x40000000,
    J3DMLF_Material_Color_AmbientOn = 0x80000000
};

struct J3DModelBlock {
    /* 0x0 */ BE(u32) mBlockType;
    /* 0x4 */ BE(u32) mBlockSize;
};

struct J3DModelFileData {
    /* 0x00 */ BE(u32) mMagic1;
    /* 0x04 */ BE(u32) mMagic2;
    /* 0x08 */ u8 _8[4];
    /* 0x0C */ BE(u32) mBlockNum;
    /* 0x10 */ u8 _10[0x10];
    /* 0x20 */ J3DModelBlock mBlocks[1];
};

struct J3DModelInfoBlock : public J3DModelBlock {
    /* 0x08 */ BE(u16) mFlags;
    /* 0x0C */ BE(u32) mPacketNum;
    /* 0x10 */ BE(u32) mVtxNum;
    /* 0x14 */ J3D_OFFSET_PTR mpHierarchy;
};

struct J3DVertexBlock : public J3DModelBlock {
    /* 0x08 */ J3D_OFFSET_PTR mpVtxAttrFmtList;
    /* 0x0C */ J3D_OFFSET_PTR mpVtxPosArray;
    /* 0x10 */ J3D_OFFSET_PTR mpVtxNrmArray;
    /* 0x14 */ J3D_OFFSET_PTR mpVtxNBTArray;
    /* 0x18 */ J3D_OFFSET_PTR mpVtxColorArray[2];
    /* 0x20 */ J3D_OFFSET_PTR mpVtxTexCoordArray[8];
};

struct J3DEnvelopeBlock : public J3DModelBlock {
    /* 0x08 */ BE(u16) mWEvlpMtxNum;
    /* 0x0C */ J3D_OFFSET_PTR mpWEvlpMixMtxNum;
    /* 0x10 */ J3D_OFFSET_PTR mpWEvlpMixIndex;
    /* 0x14 */ J3D_OFFSET_PTR mpWEvlpMixWeight;
    /* 0x18 */ J3D_OFFSET_PTR mpInvJointMtx;
};

struct J3DDrawBlock : public J3DModelBlock {
    /* 0x08 */ BE(u16) mMtxNum;
    /* 0x0C */ J3D_OFFSET_PTR mpDrawMtxFlag;
    /* 0x10 */ J3D_OFFSET_PTR mpDrawMtxIndex;
};

struct J3DJointBlock : public J3DModelBlock {
    /* 0x08 */ BE(u16) mJointNum;
    /* 0x0C */ J3D_OFFSET_PTR mpJointInitData;
    /* 0x10 */ J3D_OFFSET_PTR mpIndexTable;
    /* 0x14 */ J3D_OFFSET_PTR mpNameTable;
};  // size 0x18

struct J3DMaterialBlock : public J3DModelBlock {
    /* 0x08 */ BE(u16) mMaterialNum;
    /* 0x0C */ J3D_OFFSET_PTR mpMaterialInitData;
    /* 0x10 */ J3D_OFFSET_PTR mpMaterialID;
    /* 0x14 */ J3D_OFFSET_PTR mpNameTable;
    /* 0x18 */ J3D_OFFSET_PTR mpIndInitData;
    /* 0x1C */ J3D_OFFSET_PTR mpCullMode;
    /* 0x20 */ J3D_OFFSET_PTR mpMatColor;
    /* 0x24 */ J3D_OFFSET_PTR mpColorChanNum;
    /* 0x28 */ J3D_OFFSET_PTR mpColorChanInfo;
    /* 0x2C */ J3D_OFFSET_PTR mpAmbColor;
    /* 0x30 */ J3D_OFFSET_PTR mpLightInfo;
    /* 0x34 */ J3D_OFFSET_PTR mpTexGenNum;
    /* 0x38 */ J3D_OFFSET_PTR mpTexCoordInfo;
    /* 0x3C */ J3D_OFFSET_PTR mpTexCoord2Info;
    /* 0x40 */ J3D_OFFSET_PTR mpTexMtxInfo;
    /* 0x44 */ J3D_OFFSET_PTR field_0x44;
    /* 0x48 */ J3D_OFFSET_PTR mpTexNo;
    /* 0x4C */ J3D_OFFSET_PTR mpTevOrderInfo;
    /* 0x50 */ J3D_OFFSET_PTR mpTevColor;
    /* 0x54 */ J3D_OFFSET_PTR mpTevKColor;
    /* 0x58 */ J3D_OFFSET_PTR mpTevStageNum;
    /* 0x5C */ J3D_OFFSET_PTR mpTevStageInfo;
    /* 0x60 */ J3D_OFFSET_PTR mpTevSwapModeInfo;
    /* 0x64 */ J3D_OFFSET_PTR mpTevSwapModeTableInfo;
    /* 0x68 */ J3D_OFFSET_PTR mpFogInfo;
    /* 0x6C */ J3D_OFFSET_PTR mpAlphaCompInfo;
    /* 0x70 */ J3D_OFFSET_PTR mpBlendInfo;
    /* 0x74 */ J3D_OFFSET_PTR mpZModeInfo;
    /* 0x78 */ J3D_OFFSET_PTR mpZCompLoc;
    /* 0x7C */ J3D_OFFSET_PTR mpDither;
    /* 0x80 */ J3D_OFFSET_PTR mpNBTScaleInfo;
};

struct J3DMaterialBlock_v21 : public J3DModelBlock {
    /* 0x08 */ BE(u16) mMaterialNum;
    /* 0x0C */ J3D_OFFSET_PTR mpMaterialInitData;
    /* 0x10 */ J3D_OFFSET_PTR mpMaterialID;
    /* 0x14 */ J3D_OFFSET_PTR mpNameTable;
    /* 0x18 */ J3D_OFFSET_PTR mpCullMode;
    /* 0x1C */ J3D_OFFSET_PTR mpMatColor;
    /* 0x20 */ J3D_OFFSET_PTR mpColorChanNum;
    /* 0x24 */ J3D_OFFSET_PTR mpColorChanInfo;
    /* 0x28 */ J3D_OFFSET_PTR mpTexGenNum;
    /* 0x2C */ J3D_OFFSET_PTR mpTexCoordInfo;
    /* 0x30 */ J3D_OFFSET_PTR mpTexCoord2Info;
    /* 0x34 */ J3D_OFFSET_PTR mpTexMtxInfo;
    /* 0x38 */ J3D_OFFSET_PTR field_0x38;
    /* 0x3C */ J3D_OFFSET_PTR mpTexNo;
    /* 0x40 */ J3D_OFFSET_PTR mpTevOrderInfo;
    /* 0x44 */ J3D_OFFSET_PTR mpTevColor;
    /* 0x48 */ J3D_OFFSET_PTR mpTevKColor;
    /* 0x4C */ J3D_OFFSET_PTR mpTevStageNum;
    /* 0x50 */ J3D_OFFSET_PTR mpTevStageInfo;
    /* 0x54 */ J3D_OFFSET_PTR mpTevSwapModeInfo;
    /* 0x58 */ J3D_OFFSET_PTR mpTevSwapModeTableInfo;
    /* 0x5C */ J3D_OFFSET_PTR mpFogInfo;
    /* 0x60 */ J3D_OFFSET_PTR mpAlphaCompInfo;
    /* 0x64 */ J3D_OFFSET_PTR mpBlendInfo;
    /* 0x68 */ J3D_OFFSET_PTR mpZModeInfo;
    /* 0x6C */ J3D_OFFSET_PTR mpZCompLoc;
    /* 0x70 */ J3D_OFFSET_PTR mpDither;
    /* 0x74 */ J3D_OFFSET_PTR mpNBTScaleInfo;
};

struct J3DMaterialDLBlock : public J3DModelBlock {
    /* 0x08 */ BE(u16) mMaterialNum;
    /* 0x0C */ J3D_OFFSET_PTR mpDisplayListInit;
    /* 0x10 */ J3D_OFFSET_PTR mpPatchingInfo;
    /* 0x14 */ J3D_OFFSET_PTR mpCurrentMtxInfo;
    /* 0x18 */ J3D_OFFSET_PTR mpMaterialMode;
    /* 0x1C */ J3D_OFFSET_PTR _1C;
    /* 0x20 */ J3D_OFFSET_PTR mpNameTable;
};

struct J3DShapeBlock : public J3DModelBlock {
    /* 0x08 */ BE(u16) mShapeNum;
    /* 0x0C */ J3D_OFFSET_PTR mpShapeInitData;
    /* 0x10 */ J3D_OFFSET_PTR mpIndexTable;
    /* 0x14 */ J3D_OFFSET_PTR mpNameTable;
    /* 0x18 */ J3D_OFFSET_PTR mpVtxDescList;
    /* 0x1C */ J3D_OFFSET_PTR mpMtxTable;
    /* 0x20 */ J3D_OFFSET_PTR mpDisplayListData;
    /* 0x24 */ J3D_OFFSET_PTR mpMtxInitData;
    /* 0x28 */ J3D_OFFSET_PTR mpDrawInitData;
};

struct J3DTextureBlock : public J3DModelBlock {
    /* 0x08 */ BE(u16) mTextureNum;
    /* 0x0C */ J3D_OFFSET_PTR mpTextureRes;
    /* 0x10 */ J3D_OFFSET_PTR mpNameTable;
};

class J3DModelLoader {
public:
    J3DModelLoader();

    virtual J3DModelData* load(const void*, u32);
    virtual J3DMaterialTable* loadMaterialTable(const void*);
    virtual J3DModelData* loadBinaryDisplayList(const void*, u32);
    virtual u32 calcLoadSize(const void*, u32);
    virtual u32 calcLoadMaterialTableSize(const void*);
    virtual u32 calcLoadBinaryDisplayListSize(void const*, u32);
    virtual u16 countMaterialNum(void const*);
    virtual void setupBBoardInfo();
    virtual ~J3DModelLoader() {
    }

    virtual void readMaterial(J3DMaterialBlock const*, u32) {
    }

    virtual void readMaterial_v21(J3DMaterialBlock_v21 const*, u32) {
    }

    virtual void readMaterialTable(J3DMaterialBlock const*, u32) {
    }

    virtual void readMaterialTable_v21(J3DMaterialBlock_v21 const*, u32) {
    }

    virtual u32 calcSizeMaterial(J3DMaterialBlock const*, u32) {
        return false;
    }

    virtual u32 calcSizeMaterialTable(J3DMaterialBlock const*, u32) {
        return false;
    }

    void readInformation(J3DModelInfoBlock const*, u32);
    void readVertex(J3DVertexBlock const*);
    void readEnvelop(J3DEnvelopeBlock const*);
    void readDraw(J3DDrawBlock const*);
    void readJoint(J3DJointBlock const*);
    void readShape(J3DShapeBlock const*, u32);
    void readTexture(J3DTextureBlock const*);
    void readTextureTable(J3DTextureBlock const*);
    void readPatchedMaterial(J3DMaterialBlock const*, u32);
    void readMaterialDL(J3DMaterialDLBlock const*, u32);
    void modifyMaterial(u32);

    u32 calcSizeInformation(J3DModelInfoBlock const*, u32);
    u32 calcSizeJoint(J3DJointBlock const*);
    u32 calcSizeEnvelope(J3DEnvelopeBlock const*);
    u32 calcSizeDraw(J3DDrawBlock const*) NO_INLINE;
    u32 calcSizeShape(J3DShapeBlock const*, u32);
    u32 calcSizeTexture(J3DTextureBlock const*);
    u32 calcSizeTextureTable(J3DTextureBlock const*);
    u32 calcSizePatchedMaterial(J3DMaterialBlock const*, u32);
    u32 calcSizeMaterialDL(J3DMaterialDLBlock const*, u32);

    /* 0x04 */ J3DModelData* mpModelData;
    /* 0x08 */ J3DMaterialTable* mpMaterialTable;
    /* 0x0C */ const J3DShapeBlock* mpShapeBlock;
    /* 0x10 */ const J3DMaterialBlock* mpMaterialBlock;
    /* 0x14 */ J3DModelHierarchy* mpModelHierarchy;
    /* 0x18 */ u8 field_0x18;  // CLEANME: Change unknown JSystem members to the _XX format?
    /* 0x1C */ u8 field_0x19;
    /* 0x1A */ u16 mEnvelopeSize;
};

class J3DModelLoader_v26 : public J3DModelLoader {
public:
    J3DModelLoader_v26() {
    }

    ~J3DModelLoader_v26() {
    }

    void readMaterial(J3DMaterialBlock const*, u32);
    void readMaterialTable(J3DMaterialBlock const*, u32);
    u32 calcSizeMaterial(J3DMaterialBlock const*, u32);
    u32 calcSizeMaterialTable(J3DMaterialBlock const*, u32);
};

class J3DModelLoader_v21 : public J3DModelLoader {
public:
    inline J3DModelLoader_v21() {
    }

    ~J3DModelLoader_v21() {
    }

    void readMaterial_v21(J3DMaterialBlock_v21 const*, u32);
    void readMaterialTable_v21(J3DMaterialBlock_v21 const*, u32);
};

class J3DModelLoaderDataBase {
public:
    static J3DModelData* load(void const*, u32);
    static J3DMaterialTable* loadMaterialTable(const void*);
    static J3DModelData* loadBinaryDisplayList(const void*, u32);
};

static inline u32 getMdlDataFlag_TevStageNum(u32 flags) {
    return (flags & 0x001f0000) >> 0x10;
}

static inline u32 getMdlDataFlag_TexGenFlag(u32 flags) {
    return flags & 0x0c000000;
}

static inline u32 getMdlDataFlag_ColorFlag(u32 flags) {
    return flags & 0xc0000000;
}

static inline u32 getMdlDataFlag_PEFlag(u32 flags) {
    return flags & 0x30000000;
}

static inline u32 getMdlDataFlag_MtxLoadType(u32 flags) {
    return flags & 0x10;
}
