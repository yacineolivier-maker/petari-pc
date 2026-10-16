#include "JSystem/J3DGraphAnimator/J3DJoint.hpp"
#include "JSystem/J3DGraphLoader/J3DModelLoader.hpp"
#include "JSystem/J3DGraphAnimator/J3DModelData.hpp"
#include "JSystem/J3DGraphBase/J3DMaterial.hpp"
#include "JSystem/J3DGraphLoader/J3DJointFactory.hpp"
#include "JSystem/J3DGraphLoader/J3DMaterialFactory.hpp"
#include "JSystem/J3DGraphLoader/J3DMaterialFactory_v21.hpp"
#include "JSystem/J3DGraphLoader/J3DShapeFactory.hpp"
#include "JSystem/JKernel/JKRHeap.hpp"
#include "JSystem/JSupport/JSupport.hpp"
#include "JSystem/JUtility/JUTNameTab.hpp"
#include <stdint.h>
#if TARGET_PC
#include "helpers/endian_gx.hpp"
#endif

#if TARGET_PC
// mDiffFlag packs a material identity into 32 bits; keep host pointers from spilling into the top
// two bits (0xC0000000 is used as a marker by locked materials).
#define J3D_MATERIAL_DIFF_ID(ptr) ((u32)(((uintptr_t)(ptr) >> 4) & 0x3FFFFFFF))
#define J3D_MATERIAL_DIFF_ID_NOSHIFT(ptr) ((u32)((uintptr_t)(ptr) & 0x3FFFFFFF))
#else
#define J3D_MATERIAL_DIFF_ID(ptr) ((uintptr_t)(ptr) >> 4)
#define J3D_MATERIAL_DIFF_ID_NOSHIFT(ptr) ((uintptr_t)(ptr))
#endif

#if TARGET_PC
// File layouts read straight from BMD/BDL/BMT data must keep their Wii sizes.
static_assert(sizeof(J3DModelBlock) == 0x8);
static_assert(sizeof(J3DModelFileData) == 0x28);
static_assert(sizeof(J3DModelInfoBlock) == 0x18);
static_assert(sizeof(J3DVertexBlock) == 0x40);
static_assert(sizeof(J3DEnvelopeBlock) == 0x1C);
static_assert(sizeof(J3DDrawBlock) == 0x14);
static_assert(sizeof(J3DJointBlock) == 0x18);
static_assert(sizeof(J3DMaterialBlock) == 0x84);
static_assert(sizeof(J3DMaterialBlock_v21) == 0x78);
static_assert(sizeof(J3DMaterialDLBlock) == 0x24);
static_assert(sizeof(J3DShapeBlock) == 0x2C);
static_assert(sizeof(J3DTextureBlock) == 0x14);
static_assert(sizeof(J3DModelHierarchy) == 0x4);
static_assert(sizeof(J3DJointInitData) == 0x40);
static_assert(sizeof(J3DShapeInitData) == 0x28);
static_assert(sizeof(J3DShapeMtxInitData) == 0x8);
static_assert(sizeof(J3DShapeDrawInitData) == 0x8);
static_assert(sizeof(J3DMaterialInitData) == 0x14C);
static_assert(sizeof(J3DMaterialInitData_v21) == 0x138);
static_assert(sizeof(J3DIndInitData) == 0x138);
static_assert(sizeof(J3DPatchingInfo) == 0x10);
static_assert(sizeof(J3DDisplayListInit) == 0x8);
static_assert(sizeof(GXVtxAttrFmtList) == 0x10);
static_assert(sizeof(GXVtxDescList) == 0x8);
#endif

J3DModelLoader::J3DModelLoader()
    : mpModelData(NULL), mpMaterialTable(NULL), mpShapeBlock(NULL), mpMaterialBlock(NULL), mpModelHierarchy(NULL), field_0x18(0), mEnvelopeSize(0) {
    /* empty function */
}

J3DModelData* J3DModelLoaderDataBase::load(void const* i_data, u32 i_flags) {
    if (i_data == NULL) {
        return NULL;
    }
#if TARGET_PC
    const J3DModelFileData* header = (const J3DModelFileData*)i_data;
    const u32 magic1 = header->mMagic1;
    const u32 magic2 = header->mMagic2;
    if (magic1 == 'J3D1' && magic2 == 'bmd1') {
        return NULL;
    }
    if (magic1 == 'J3D2' && magic2 == 'bmd2') {
        J3DModelLoader_v21 loader;
        return loader.load(i_data, i_flags);
    }
    if (magic1 == 'J3D2' && magic2 == 'bmd3') {
#else
    if (*(u32*)i_data == 'J3D1' && *(u32*)((uintptr_t)i_data + 4) == 'bmd1') {
        return NULL;
    }
    if (*(u32*)i_data == 'J3D2' && *(u32*)((uintptr_t)i_data + 4) == 'bmd2') {
        J3DModelLoader_v21 loader;
        return loader.load(i_data, i_flags);
    }
    if (*(u32*)i_data == 'J3D2' && *(u32*)((uintptr_t)i_data + 4) == 'bmd3') {
#endif
        J3DModelLoader_v26 loader;
        return loader.load(i_data, i_flags);
    }
    return NULL;
}

J3DMaterialTable* J3DModelLoaderDataBase::loadMaterialTable(const void* data) {
    if (data == NULL) {
        return NULL;
    }
#if TARGET_PC
    const J3DModelFileData* header = reinterpret_cast< const J3DModelFileData* >(data);
    if (header->mMagic1 == 'J3D2' && header->mMagic2 == 'bmt3') {
#else
    if (reinterpret_cast< const u32* >(data)[0] == 'J3D2' && reinterpret_cast< const u32* >(data)[1] == 'bmt3') {
#endif
        J3DModelLoader_v26 loader;
        return loader.loadMaterialTable(data);
    }
    return NULL;
}

J3DModelData* J3DModelLoaderDataBase::loadBinaryDisplayList(const void* data, u32 flags) {
    if (data == NULL) {
        return NULL;
    }
#if TARGET_PC
    const J3DModelFileData* header = reinterpret_cast< const J3DModelFileData* >(data);
    if (header->mMagic1 == 'J3D2' && (header->mMagic2 == 'bdl3' || header->mMagic2 == 'bdl4')) {
#else
    if (reinterpret_cast< const u32* >(data)[0] == 'J3D2' &&
        (reinterpret_cast< const u32* >(data)[1] == 'bdl3' || reinterpret_cast< const u32* >(data)[1] == 'bdl4')) {
#endif
        J3DModelLoader_v26 loader;
        return loader.loadBinaryDisplayList(data, flags);
    }
    return NULL;
}

J3DModelData* J3DModelLoader::load(void const* i_data, u32 i_flags) {
    JKRGetCurrentHeap()->getTotalFreeSize();
    mpModelData = new J3DModelData();
    mpModelData->clear();
    mpModelData->mpRawData = i_data;
    mpModelData->setModelDataType(0);
    mpMaterialTable = &mpModelData->mMaterialTable;
    J3DModelFileData const* data = (J3DModelFileData*)i_data;
    J3DModelBlock const* block = data->mBlocks;
    for (u32 block_no = 0; block_no < data->mBlockNum; block_no++) {
        switch (block->mBlockType) {
        case 'INF1':
            readInformation((J3DModelInfoBlock*)block, (s32)i_flags);
            break;
        case 'VTX1':
            readVertex((J3DVertexBlock*)block);
            break;
        case 'EVP1':
            readEnvelop((J3DEnvelopeBlock*)block);
            break;
        case 'DRW1':
            readDraw((J3DDrawBlock*)block);
            break;
        case 'JNT1':
            readJoint((J3DJointBlock*)block);
            break;
        case 'MAT3':
            readMaterial((J3DMaterialBlock*)block, (s32)i_flags);
            break;
        case 'MAT2':
            readMaterial_v21((J3DMaterialBlock_v21*)block, (s32)i_flags);
            break;
        case 'SHP1':
            readShape((J3DShapeBlock*)block, (s32)i_flags);
            break;
        case 'TEX1':
            readTexture((J3DTextureBlock*)block);
            break;
        default:
            break;
        }
        block = (J3DModelBlock*)((uintptr_t)block + block->mBlockSize);
    }
    J3DModelHierarchy const* hierarchy = mpModelData->getHierarchy();
    mpModelData->makeHierarchy(NULL, &hierarchy);
    mpModelData->getShapeTable()->sortVcdVatCmd();
    mpModelData->getJointTree().findImportantMtxIndex();
    setupBBoardInfo();
    if (mpModelData->getFlag() & 0x100) {
        for (u16 shape_no = 0; shape_no < mpModelData->getShapeNum(); shape_no++) {
            mpModelData->getShapeNodePointer(shape_no)->onFlag(0x200);
        }
    }
    return mpModelData;
}

J3DMaterialTable* J3DModelLoader::loadMaterialTable(void const* i_data) {
    mpMaterialTable = new J3DMaterialTable();
    mpMaterialTable->clear();
    J3DModelFileData const* data = (J3DModelFileData*)i_data;
    J3DModelBlock const* block = data->mBlocks;
    for (u32 block_no = 0; block_no < data->mBlockNum; block_no++) {
        switch (block->mBlockType) {
        case 'MAT3':
            readMaterialTable((J3DMaterialBlock*)block, 0x51100000);
            break;
        case 'MAT2':
            readMaterialTable_v21((J3DMaterialBlock_v21*)block, 0x51100000);
            break;
        case 'TEX1':
            readTextureTable((J3DTextureBlock*)block);
            break;
        default:
            break;
        }
        block = (J3DModelBlock*)((uintptr_t)block + block->mBlockSize);
    }
    if (mpMaterialTable->mTexture == NULL) {
        mpMaterialTable->mTexture = new J3DTexture(0, NULL);
    }
    return mpMaterialTable;
}

J3DModelData* J3DModelLoader::loadBinaryDisplayList(void const* i_data, u32 i_flags) {
    mpModelData = new J3DModelData();
    mpModelData->clear();
    mpModelData->mpRawData = i_data;
    mpModelData->setModelDataType(1);
    mpMaterialTable = &mpModelData->mMaterialTable;
    J3DModelFileData const* data = (J3DModelFileData*)i_data;
    J3DModelBlock const* block = data->mBlocks;
    for (u32 block_no = 0; block_no < data->mBlockNum; block_no++) {
        s32 flags;
        switch (block->mBlockType) {
        case 'INF1':
            flags = i_flags;
            readInformation((J3DModelInfoBlock*)block, flags);
            break;
        case 'VTX1':
            readVertex((J3DVertexBlock*)block);
            break;
        case 'EVP1':
            readEnvelop((J3DEnvelopeBlock*)block);
            break;
        case 'DRW1':
            readDraw((J3DDrawBlock*)block);
            break;
        case 'JNT1':
            readJoint((J3DJointBlock*)block);
            break;
        case 'SHP1':
            readShape((J3DShapeBlock*)block, i_flags);
            break;
        case 'TEX1':
            readTexture((J3DTextureBlock*)block);
            break;
        case 'MDL3':
            readMaterialDL((J3DMaterialDLBlock*)block, i_flags);
            modifyMaterial(i_flags);
            break;
        case 'MAT3':
            flags = 0x50100000;
            flags |= (i_flags & 0x3000000);
            mpMaterialBlock = (J3DMaterialBlock*)block;
            if (((u32)i_flags & 0x3000) == 0) {
                readMaterial((J3DMaterialBlock*)block, flags);
            } else if (((u32)i_flags & 0x3000) == 0x2000) {
                readPatchedMaterial((J3DMaterialBlock*)block, flags);
            }
            break;
        default:
            break;
        }
        block = (J3DModelBlock*)((uintptr_t)block + block->mBlockSize);
    }
    J3DModelHierarchy const* hierarchy = mpModelData->getHierarchy();
    mpModelData->makeHierarchy(NULL, &hierarchy);
    mpModelData->getShapeTable()->sortVcdVatCmd();
    mpModelData->getJointTree().findImportantMtxIndex();
    setupBBoardInfo();
    mpModelData->indexToPtr();
    return mpModelData;
}

void J3DModelLoader::setupBBoardInfo() {
    for (u16 i = 0; i < mpModelData->getJointNum(); i++) {
        J3DMaterial* mesh = mpModelData->getJointNodePointer(i)->getMesh();
        if (mesh != NULL) {
            u32 shape_index = mesh->getShape()->getIndex();
            BE(u16)* index_table = JSUConvertOffsetToPtr< BE(u16) >(mpShapeBlock, (uintptr_t)mpShapeBlock->mpIndexTable);
            J3DShapeInitData* shape_init_data = JSUConvertOffsetToPtr< J3DShapeInitData >(mpShapeBlock, (uintptr_t)mpShapeBlock->mpShapeInitData);
            J3DJoint* joint;
            switch (shape_init_data[index_table[shape_index]].mShapeMtxType) {
            case 0:
                joint = mpModelData->getJointNodePointer(i);
                joint->setMtxType(0);
                break;
            case 1:
                joint = mpModelData->getJointNodePointer(i);
                joint->setMtxType(1);
                mpModelData->mbHasBillboard = true;
                break;
            case 2:
                joint = mpModelData->getJointNodePointer(i);
                joint->setMtxType(2);
                mpModelData->mbHasBillboard = true;
                break;
            case 3:
                joint = mpModelData->getJointNodePointer(i);
                joint->setMtxType(0);
                break;
            default:
                break;
            }
        }
    }
}

void J3DModelLoader::readInformation(J3DModelInfoBlock const* i_block, u32 i_flags) {
    mpModelData->mFlags = i_flags | i_block->mFlags;
    mpModelData->getJointTree().setFlag(mpModelData->mFlags);
    J3DMtxCalc* mtx_calc = NULL;
    switch (mpModelData->mFlags & 0xf) {
    case 0:
        mtx_calc = new J3DMtxCalcNoAnm< J3DMtxCalcCalcTransformBasic, J3DMtxCalcJ3DSysInitBasic >();
        break;
    case 1:
        mtx_calc = new J3DMtxCalcNoAnm< J3DMtxCalcCalcTransformSoftimage, J3DMtxCalcJ3DSysInitSoftimage >();
        break;
    case 2:
        mtx_calc = new J3DMtxCalcNoAnm< J3DMtxCalcCalcTransformMaya, J3DMtxCalcJ3DSysInitMaya >();
        break;
    }
    mpModelData->setBasicMtxCalc(mtx_calc);
    mpModelData->getVertexData().mPacketNum = i_block->mPacketNum;
    mpModelData->getVertexData().mVtxNum = i_block->mVtxNum;
    mpModelData->setHierarchy(JSUConvertOffsetToPtr< J3DModelHierarchy >(i_block, i_block->mpHierarchy));
}

inline J3DMtxCalcNoAnmBase::J3DMtxCalcNoAnmBase() {
}

#if TARGET_PC
// GX and J3DShape read the attribute format list as host structs. The file list stays untouched
// (big-endian); the loader works on a host-endian copy allocated with the model data.
static GXVtxAttrFmtList* J3DCopyVtxAttrFmtList(const BE(GXVtxAttrFmtList)* i_src) {
    if (i_src == NULL) {
        return NULL;
    }
    u32 num = 0;
    while (i_src[num].attr != GX_VA_NULL) {
        num++;
    }
    GXVtxAttrFmtList* dst = new GXVtxAttrFmtList[num + 1];
    for (u32 i = 0; i <= num; i++) {
        dst[i].attr = i_src[i].attr;
        dst[i].cnt = i_src[i].cnt;
        dst[i].type = i_src[i].type;
        dst[i].frac = i_src[i].frac;
    }
    return dst;
}

// Records the byte size of every VTX1 array (they are laid out back to back, in J3DVertexBlock
// order, up to the end of the block). Aurora needs it to upload the big-endian arrays.
static void J3DReadVtxArraySizes(const J3DVertexBlock* i_block, J3DVertexData& vertex_data) {
    static const GXAttr sAttrOrder[13] = {
        GX_VA_POS, GX_VA_NRM, GX_VA_NBT, GX_VA_CLR0, GX_VA_CLR1, GX_VA_TEX0, GX_VA_TEX1,
        GX_VA_TEX2, GX_VA_TEX3, GX_VA_TEX4, GX_VA_TEX5, GX_VA_TEX6, GX_VA_TEX7,
    };
    const BE(u32)* offsets = &i_block->mpVtxPosArray;
    for (int i = 0; i < 13; i++) {
        u32 size = 0;
        u32 start = offsets[i];
        if (start != 0) {
            u32 end = i_block->mBlockSize;
            for (int j = i + 1; j < 13; j++) {
                if (offsets[j] != 0) {
                    end = offsets[j];
                    break;
                }
            }
            size = end > start ? end - start : 0;
        }
        vertex_data.setVtxArrSize(sAttrOrder[i], size);
    }
}
#endif

static _GXCompType getFmtType(_GXVtxAttrFmtList* i_fmtList, _GXAttr i_attr) {
    for (; i_fmtList->attr != GX_VA_NULL; i_fmtList++) {
        if (i_fmtList->attr == i_attr) {
            return i_fmtList->type;
        }
    }
    return GX_F32;
}

void J3DModelLoader::readVertex(J3DVertexBlock const* i_block) {
    J3DVertexData& vertex_data = mpModelData->getVertexData();
#if TARGET_PC
    vertex_data.mVtxAttrFmtList = J3DCopyVtxAttrFmtList(JSUConvertOffsetToPtr< BE(GXVtxAttrFmtList) >(i_block, i_block->mpVtxAttrFmtList));
#else
    vertex_data.mVtxAttrFmtList = JSUConvertOffsetToPtr< GXVtxAttrFmtList >(i_block, i_block->mpVtxAttrFmtList);
#endif
    vertex_data.mVtxPosArray = JSUConvertOffsetToPtr< void >(i_block, i_block->mpVtxPosArray);
    vertex_data.mVtxNrmArray = JSUConvertOffsetToPtr< void >(i_block, i_block->mpVtxNrmArray);
    vertex_data.mVtxNBTArray = JSUConvertOffsetToPtr< void >(i_block, i_block->mpVtxNBTArray);
    for (int i = 0; i < 2; i++) {
        vertex_data.mVtxColorArray[i] = (GXColor*)JSUConvertOffsetToPtr< void >(i_block, i_block->mpVtxColorArray[i]);
    }
    for (int i = 0; i < 8; i++) {
        vertex_data.mVtxTexCoordArray[i] = JSUConvertOffsetToPtr< void >(i_block, i_block->mpVtxTexCoordArray[i]);
    }

    _GXCompType nrm_type = getFmtType(vertex_data.mVtxAttrFmtList, GX_VA_NRM);
    u32 nrm_size = nrm_type == GX_F32 ? 12 : 6;

    void* nrm_end = NULL;
    if (vertex_data.mVtxNBTArray != NULL) {
        nrm_end = vertex_data.mVtxNBTArray;
    } else if (vertex_data.mVtxColorArray[0] != NULL) {
        nrm_end = vertex_data.mVtxColorArray[0];
    } else if (vertex_data.mVtxTexCoordArray[0] != NULL) {
        nrm_end = vertex_data.mVtxTexCoordArray[0];
    }

    if (vertex_data.mVtxNrmArray == NULL) {
        vertex_data.mNrmNum = 0;
    } else if (nrm_end != NULL) {
        vertex_data.mNrmNum = ((uintptr_t)nrm_end - (uintptr_t)vertex_data.mVtxNrmArray) / nrm_size + 1;
    } else {
        vertex_data.mNrmNum = (i_block->mBlockSize - (uintptr_t)i_block->mpVtxNrmArray) / nrm_size + 1;
    }

    void* color0_end = NULL;
    if (vertex_data.mVtxColorArray[1] != NULL) {
        color0_end = vertex_data.mVtxColorArray[1];
    } else if (vertex_data.mVtxTexCoordArray[0] != NULL) {
        color0_end = vertex_data.mVtxTexCoordArray[0];
    }

    if (vertex_data.mVtxColorArray[0] == NULL) {
        vertex_data.mColNum = 0;
    } else if (color0_end != NULL) {
        vertex_data.mColNum = ((uintptr_t)color0_end - (uintptr_t)vertex_data.mVtxColorArray[0]) / 4 + 1;
    } else {
        vertex_data.mColNum = (i_block->mBlockSize - (uintptr_t)i_block->mpVtxColorArray[0]) / 4 + 1;
    }

    if (vertex_data.mVtxTexCoordArray[0] == NULL) {
        vertex_data.mTexCoordNum = 0;
    } else {
        vertex_data.mTexCoordNum = (i_block->mBlockSize - (uintptr_t)i_block->mpVtxTexCoordArray[0]) / 8 + 1;
    }

#if TARGET_PC
    J3DReadVtxArraySizes(i_block, vertex_data);
#endif
}

void J3DModelLoader::readEnvelop(J3DEnvelopeBlock const* i_block) {
    mpModelData->getJointTree().mWEvlpMtxNum = i_block->mWEvlpMtxNum;
    mpModelData->getJointTree().mWEvlpMixMtxNum = JSUConvertOffsetToPtr< u8 >(i_block, i_block->mpWEvlpMixMtxNum);
    mpModelData->getJointTree().mWEvlpMixMtxIndex = JSUConvertOffsetToPtr< BE(u16) >(i_block, i_block->mpWEvlpMixIndex);
    mpModelData->getJointTree().mWEvlpMixWeight = JSUConvertOffsetToPtr< BE(f32) >(i_block, i_block->mpWEvlpMixWeight);
    mpModelData->getJointTree().mInvJointMtx = JSUConvertOffsetToPtr< BE(Mtx) >(i_block, i_block->mpInvJointMtx);
}

void J3DModelLoader::readDraw(J3DDrawBlock const* i_block) {
    u16 i;
    J3DModelData* modelData = mpModelData;
    modelData->getJointTree().mDrawMtxData.mEntryNum = i_block->mMtxNum - mpModelData->getJointTree().mWEvlpMtxNum;
    modelData->getJointTree().mDrawMtxData.mDrawMtxFlag = JSUConvertOffsetToPtr< u8 >(i_block, i_block->mpDrawMtxFlag);
    modelData->getJointTree().mDrawMtxData.mDrawMtxIndex = JSUConvertOffsetToPtr< BE(u16) >(i_block, i_block->mpDrawMtxIndex);
    for (i = 0; i < modelData->getJointTree().mDrawMtxData.mEntryNum; i++) {
        if (modelData->getJointTree().mDrawMtxData.mDrawMtxFlag[i] == 1) {
            break;
        }
    }
    modelData->getJointTree().mDrawMtxData.mDrawFullWgtMtxNum = i;
    mpModelData->getJointTree().mWEvlpImportantMtxIdx = new u16[modelData->getJointTree().mDrawMtxData.mEntryNum];
}

void J3DModelLoader::readJoint(J3DJointBlock const* i_block) {
    J3DJointFactory factory(*i_block);
    mpModelData->getJointTree().mJointNum = i_block->mJointNum;
    if (i_block->mpNameTable != NULL) {
        mpModelData->getJointTree().mJointName = new JUTNameTab(JSUConvertOffsetToPtr< ResNTAB >(i_block, i_block->mpNameTable));
    } else {
        mpModelData->getJointTree().mJointName = NULL;
    }
    mpModelData->getJointTree().mJointNodePointer = new J3DJoint*[mpModelData->getJointTree().mJointNum];
    for (u16 i = 0; i < mpModelData->getJointTree().getJointNum(); i++) {
        mpModelData->getJointTree().mJointNodePointer[i] = factory.create(i);
    }
}

void J3DModelLoader_v26::readMaterial(J3DMaterialBlock const* i_block, u32 i_flags) {
    J3DMaterialFactory factory(*i_block);
    mpMaterialTable->mMaterialNum = i_block->mMaterialNum;
    mpMaterialTable->mUniqueMatNum = factory.countUniqueMaterials();
    if (i_block->mpNameTable != NULL) {
        mpMaterialTable->mMaterialName = new JUTNameTab(JSUConvertOffsetToPtr< ResNTAB >(i_block, i_block->mpNameTable));
    } else {
        mpMaterialTable->mMaterialName = NULL;
    }
    mpMaterialTable->mMaterialNodePointer = new J3DMaterial*[mpMaterialTable->mMaterialNum];
    if (i_flags & 0x200000) {
        mpMaterialTable->field_0x10 = new (0x20) J3DMaterial[mpMaterialTable->mUniqueMatNum];
    } else {
        mpMaterialTable->field_0x10 = NULL;
    }
    if (i_flags & 0x200000) {
        for (u16 i = 0; i < mpMaterialTable->mUniqueMatNum; i++) {
            factory.create(&mpMaterialTable->field_0x10[i], J3DMaterialFactory::MATERIAL_TYPE_NORMAL, i, i_flags);
            mpMaterialTable->field_0x10[i].mDiffFlag = J3D_MATERIAL_DIFF_ID(&mpMaterialTable->field_0x10[i]);
        }
    }
    for (u16 i = 0; i < mpMaterialTable->mMaterialNum; i++) {
        mpMaterialTable->mMaterialNodePointer[i] = factory.create(NULL, J3DMaterialFactory::MATERIAL_TYPE_NORMAL, i, i_flags);
    }
    if (i_flags & 0x200000) {
        for (u16 i = 0; i < mpMaterialTable->mMaterialNum; i++) {
            mpMaterialTable->mMaterialNodePointer[i]->mDiffFlag = J3D_MATERIAL_DIFF_ID(&mpMaterialTable->field_0x10[factory.getMaterialID(i)]);
            mpMaterialTable->mMaterialNodePointer[i]->mpOrigMaterial = &mpMaterialTable->field_0x10[factory.getMaterialID(i)];
        }
    } else {
        for (u16 i = 0; i < mpMaterialTable->mMaterialNum; i++) {
            mpMaterialTable->mMaterialNodePointer[i]->mDiffFlag = J3D_MATERIAL_DIFF_ID(mpMaterialTable->mMaterialNodePointer) + factory.getMaterialID(i);
        }
    }
}

void J3DModelLoader_v21::readMaterial_v21(J3DMaterialBlock_v21 const* i_block, u32 i_flags) {
    J3DMaterialFactory_v21 factory(*i_block);
    mpMaterialTable->mMaterialNum = i_block->mMaterialNum;
    mpMaterialTable->mUniqueMatNum = factory.countUniqueMaterials();
    if (i_block->mpNameTable != NULL) {
        mpMaterialTable->mMaterialName = new JUTNameTab(JSUConvertOffsetToPtr< ResNTAB >(i_block, i_block->mpNameTable));
    } else {
        mpMaterialTable->mMaterialName = NULL;
    }
    mpMaterialTable->mMaterialNodePointer = new J3DMaterial*[mpMaterialTable->mMaterialNum];
    if (i_flags & 0x200000) {
        mpMaterialTable->field_0x10 = new (0x20) J3DMaterial[mpMaterialTable->mUniqueMatNum];
    } else {
        mpMaterialTable->field_0x10 = NULL;
    }
    if (i_flags & 0x200000) {
        for (u16 i = 0; i < mpMaterialTable->mUniqueMatNum; i++) {
            factory.create(&mpMaterialTable->field_0x10[i], i, i_flags);
            mpMaterialTable->field_0x10[i].mDiffFlag = J3D_MATERIAL_DIFF_ID(&mpMaterialTable->field_0x10[i]);
        }
    }
    for (u16 i = 0; i < mpMaterialTable->mMaterialNum; i++) {
        mpMaterialTable->mMaterialNodePointer[i] = factory.create(NULL, i, i_flags);
    }
    if (i_flags & 0x200000) {
        for (u16 i = 0; i < mpMaterialTable->mMaterialNum; i++) {
            mpMaterialTable->mMaterialNodePointer[i]->mDiffFlag = J3D_MATERIAL_DIFF_ID(&mpMaterialTable->field_0x10[factory.getMaterialID(i)]);
            mpMaterialTable->mMaterialNodePointer[i]->mpOrigMaterial = &mpMaterialTable->field_0x10[factory.getMaterialID(i)];
        }
    } else {
        for (u16 i = 0; i < mpMaterialTable->mMaterialNum; i++) {
            mpMaterialTable->mMaterialNodePointer[i]->mDiffFlag = 0xc0000000;
        }
    }
}

void J3DModelLoader::readShape(J3DShapeBlock const* i_block, u32 i_flags) {
    mpShapeBlock = i_block;
    J3DShapeTable* shape_table = mpModelData->getShapeTable();
    J3DShapeFactory factory(*i_block);
    shape_table->mShapeNum = i_block->mShapeNum;
    if (i_block->mpNameTable != NULL) {
        shape_table->mShapeName = new JUTNameTab(JSUConvertOffsetToPtr< ResNTAB >(i_block, i_block->mpNameTable));
    } else {
        shape_table->mShapeName = NULL;
    }
    shape_table->mShapeNodePointer = new J3DShape*[shape_table->mShapeNum];
    factory.allocVcdVatCmdBuffer(shape_table->mShapeNum);
    J3DModelHierarchy const* hierarchy_entry = mpModelData->getHierarchy();
    GXVtxDescList* vtx_desc_list = NULL;
    for (; hierarchy_entry->mType != 0; hierarchy_entry++) {
        if (hierarchy_entry->mType == 0x12) {
            shape_table->mShapeNodePointer[hierarchy_entry->mValue] = factory.create(hierarchy_entry->mValue, i_flags, vtx_desc_list);
            vtx_desc_list = factory.getVtxDescList(hierarchy_entry->mValue);
        }
    }
}

void J3DModelLoader::readTexture(J3DTextureBlock const* i_block) {
    ResTIMG* texture_res;
    u32 texture_num = i_block->mTextureNum;
    texture_res = JSUConvertOffsetToPtr< ResTIMG >(i_block, i_block->mpTextureRes);
    if (i_block->mpNameTable != NULL) {
        mpMaterialTable->mTextureName = new JUTNameTab(JSUConvertOffsetToPtr< ResNTAB >(i_block, i_block->mpNameTable));
    } else {
        mpMaterialTable->mTextureName = NULL;
    }
    mpMaterialTable->mTexture = new J3DTexture(texture_num, texture_res);
}

void J3DModelLoader_v26::readMaterialTable(J3DMaterialBlock const* i_block, u32 i_flags) {
    J3DMaterialFactory factory(*i_block);
    mpMaterialTable->mMaterialNum = i_block->mMaterialNum;
    if (i_block->mpNameTable != NULL) {
        mpMaterialTable->mMaterialName = new JUTNameTab(JSUConvertOffsetToPtr< ResNTAB >(i_block, i_block->mpNameTable));
    } else {
        mpMaterialTable->mMaterialName = NULL;
    }
    mpMaterialTable->mMaterialNodePointer = new J3DMaterial*[mpMaterialTable->mMaterialNum];
    for (u16 i = 0; i < mpMaterialTable->mMaterialNum; i++) {
        mpMaterialTable->mMaterialNodePointer[i] = factory.create(NULL, J3DMaterialFactory::MATERIAL_TYPE_NORMAL, i, i_flags);
    }
    for (u16 i = 0; i < mpMaterialTable->mMaterialNum; i++) {
        mpMaterialTable->mMaterialNodePointer[i]->mDiffFlag = J3D_MATERIAL_DIFF_ID_NOSHIFT(mpMaterialTable->mMaterialNodePointer) + factory.getMaterialID(i);
    }
}

void J3DModelLoader_v21::readMaterialTable_v21(J3DMaterialBlock_v21 const* i_block, u32 i_flags) {
    J3DMaterialFactory_v21 factory(*i_block);
    mpMaterialTable->mMaterialNum = i_block->mMaterialNum;
    if (i_block->mpNameTable != NULL) {
        mpMaterialTable->mMaterialName = new JUTNameTab(JSUConvertOffsetToPtr< ResNTAB >(i_block, i_block->mpNameTable));
    } else {
        mpMaterialTable->mMaterialName = NULL;
    }
    mpMaterialTable->mMaterialNodePointer = new J3DMaterial*[mpMaterialTable->mMaterialNum];
    for (u16 i = 0; i < mpMaterialTable->mMaterialNum; i++) {
        mpMaterialTable->mMaterialNodePointer[i] = factory.create(NULL, i, i_flags);
    }
    for (u16 i = 0; i < mpMaterialTable->mMaterialNum; i++) {
        mpMaterialTable->mMaterialNodePointer[i]->mDiffFlag = J3D_MATERIAL_DIFF_ID(mpMaterialTable->mMaterialNodePointer) + factory.getMaterialID(i);
    }
}

void J3DModelLoader::readTextureTable(J3DTextureBlock const* i_block) {
    ResTIMG* texture_res;
    u32 texture_num = i_block->mTextureNum;
    texture_res = JSUConvertOffsetToPtr< ResTIMG >(i_block, i_block->mpTextureRes);
    if (i_block->mpNameTable != NULL) {
        mpMaterialTable->mTextureName = new JUTNameTab(JSUConvertOffsetToPtr< ResNTAB >(i_block, i_block->mpNameTable));
    } else {
        mpMaterialTable->mTextureName = NULL;
    }
    mpMaterialTable->mTexture = new J3DTexture(texture_num, texture_res);
}

void J3DModelLoader::readPatchedMaterial(J3DMaterialBlock const* i_block, u32 i_flags) {
    J3DMaterialFactory factory(*i_block);
    mpMaterialTable->mMaterialNum = i_block->mMaterialNum;
    mpMaterialTable->mUniqueMatNum = factory.countUniqueMaterials();
    if (i_block->mpNameTable != NULL) {
        mpMaterialTable->mMaterialName = new JUTNameTab(JSUConvertOffsetToPtr< ResNTAB >(i_block, i_block->mpNameTable));
    } else {
        mpMaterialTable->mMaterialName = NULL;
    }
    mpMaterialTable->mMaterialNodePointer = new J3DMaterial*[mpMaterialTable->mMaterialNum];
    mpMaterialTable->field_0x10 = NULL;
    for (u16 i = 0; i < mpMaterialTable->mMaterialNum; i++) {
        mpMaterialTable->mMaterialNodePointer[i] = factory.create(NULL, J3DMaterialFactory::MATERIAL_TYPE_PATCHED, i, i_flags);
        mpMaterialTable->mMaterialNodePointer[i]->mDiffFlag = J3D_MATERIAL_DIFF_ID(mpMaterialTable->mMaterialNodePointer) + factory.getMaterialID(i);
    }
}

void J3DModelLoader::readMaterialDL(J3DMaterialDLBlock const* i_block, u32 i_flags) {
    J3DMaterialFactory factory(*i_block);
    if (mpMaterialTable->mMaterialNum == 0) {
        mpMaterialTable->field_0x1c = 1;
        mpMaterialTable->mMaterialNum = i_block->mMaterialNum;
        mpMaterialTable->mUniqueMatNum = i_block->mMaterialNum;
        if (i_block->mpNameTable != NULL) {
            mpMaterialTable->mMaterialName = new JUTNameTab(JSUConvertOffsetToPtr< ResNTAB >(i_block, i_block->mpNameTable));
        } else {
            mpMaterialTable->mMaterialName = NULL;
        }
        mpMaterialTable->mMaterialNodePointer = new J3DMaterial*[mpMaterialTable->mMaterialNum];
        mpMaterialTable->field_0x10 = NULL;
        for (u16 i = 0; i < mpMaterialTable->mMaterialNum; i++) {
            mpMaterialTable->mMaterialNodePointer[i] = factory.create(NULL, J3DMaterialFactory::MATERIAL_TYPE_LOCKED, i, i_flags);
        }
        for (u16 i = 0; i < mpMaterialTable->mMaterialNum; i++) {
            mpMaterialTable->mMaterialNodePointer[i]->mDiffFlag = 0xc0000000;
        }
    } else {
        for (u16 i = 0; i < mpMaterialTable->mMaterialNum; i++) {
            mpMaterialTable->mMaterialNodePointer[i] =
                factory.create(mpMaterialTable->mMaterialNodePointer[i], J3DMaterialFactory::MATERIAL_TYPE_LOCKED, i, i_flags);
        }
    }
}

void J3DModelLoader::modifyMaterial(u32 i_flags) {
    if (i_flags & 0x2000) {
        J3DMaterialFactory factory(*mpMaterialBlock);
        for (u16 i = 0; i < mpMaterialTable->mMaterialNum; i++) {
            factory.modifyPatchedCurrentMtx(mpMaterialTable->mMaterialNodePointer[i], i);
        }
    }
}
