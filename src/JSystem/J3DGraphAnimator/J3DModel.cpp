#include "JSystem/J3DGraphAnimator/J3DModel.hpp"
#include "JSystem/J3DGraphAnimator/J3DJoint.hpp"
#include "JSystem/J3DGraphAnimator/J3DMaterialAnm.hpp"
#include "JSystem/J3DGraphBase/J3DMaterial.hpp"
#include "JSystem/J3DGraphBase/J3DShapeMtx.hpp"

void J3DModel::initialize() {
    mModelData = NULL;
    mFlags = 0;
    mDiffFlag = 0;
    mCalcCallBack = NULL;
    mUserArea = 0;

    mBaseScale.x = 1.0f;
    mBaseScale.y = 1.0f;
    mBaseScale.z = 1.0f;

    PSMTXIdentity(mBaseTransformMtx);
    PSMTXIdentity(mInternalView);

    mMtxBuffer = NULL;
    mMatPacket = NULL;
    mShapePacket = NULL;
    mDeformData = NULL;
    mSkinDeform = NULL;
    mVtxColorCalc = NULL;
    mUnkCalc1 = NULL;
    mUnkCalc2 = NULL;
}

s32 J3DModel::entryModelData(J3DModelData* pModelData, u32 mdlFlags, u32 mtxNum) {
    int ret = kJ3DError_Success;

    mModelData = pModelData;
    mMtxBuffer = new J3DMtxBuffer();

    ret = mMtxBuffer->create(pModelData, mtxNum);
    if (ret != kJ3DError_Success) {
        return ret;
    }

    ret = createShapePacket(pModelData);
    if (ret != kJ3DError_Success) {
        return ret;
    }

    ret = createMatPacket(pModelData, mdlFlags);
    if (ret != kJ3DError_Success) {
        return ret;
    }

    mVertexBuffer.setVertexData(&pModelData->getVertexData());
    prepareShapePackets();

    if (mdlFlags & J3DMdlFlag_Unk1) {
        onFlag(J3DMdlFlag_Unk1);
    }
    if (mdlFlags & J3DMdlFlag_UseDefaultJ3D) {
        onFlag(J3DMdlFlag_UseDefaultJ3D);
    }
    if (mdlFlags & J3DMdlFlag_EnableLOD) {
        onFlag(J3DMdlFlag_EnableLOD);
    }
    if (mdlFlags & J3DMdlFlag_UseSharedDL) {
        onFlag(J3DMdlFlag_UseSharedDL);
    }
    if (mdlFlags & J3DMdlFlag_UseSingleDL) {
        onFlag(J3DMdlFlag_UseSingleDL);
    }
    if (mdlFlags & J3DMdlFlag_DifferedDLBuffer) {
        onFlag(J3DMdlFlag_DifferedDLBuffer);
    }
    if (mdlFlags & J3DMdlFlag_SkinPosCpu) {
        onFlag(J3DMdlFlag_SkinPosCpu);
    }
    if (mdlFlags & J3DMdlFlag_SkinNrmCpu) {
        onFlag(J3DMdlFlag_SkinNrmCpu);
    }

    return kJ3DError_Success;
}

s32 J3DModel::createShapePacket(J3DModelData* pModelData) {
    if (pModelData->getShapeNum() != 0) {
        mShapePacket = new J3DShapePacket[pModelData->getShapeNum()];

        for (int i = 0; i < pModelData->getShapeNum(); i++) {
            mShapePacket[i].setShape(pModelData->getShapeNodePointer(i));
            mShapePacket[i].setModel(this);
        }
    }

    return kJ3DError_Success;
}

s32 J3DModel::createMatPacket(J3DModelData* pModelData, u32 mdlFlags) {
    if (pModelData->getMaterialNum() != 0) {
        mMatPacket = new J3DMatPacket[pModelData->getMaterialNum()];
    }

    u16 matNum = pModelData->getMaterialNum();
    for (u16 i = 0; i < matNum; i++) {
        J3DMaterial* materialNode = pModelData->getMaterialNodePointer(i);
        J3DMatPacket* matPacket = mMatPacket + i;
        J3DShapePacket* shapePacket = mShapePacket + materialNode->getShape()->getIndex();

        matPacket->setMaterial(materialNode);
        matPacket->setInitShapePacket(shapePacket);
        matPacket->addShapePacket(shapePacket);
        matPacket->setTexture(pModelData->getTexture());
        matPacket->setMaterialID(materialNode->mDiffFlag);

        if (pModelData->getModelDataType() == 1) {
            matPacket->lock();
        }

        if (mdlFlags & J3DMdlFlag_DifferedDLBuffer) {
            J3DDisplayListObj* dlobj = materialNode->getSharedDisplayListObj();
            matPacket->setDisplayListObj(dlobj);
        } else {
            if (pModelData->getModelDataType() == 1) {
                if (mdlFlags & J3DMdlFlag_UseSingleDL) {
                    matPacket->mpDisplayListObj = materialNode->getSharedDisplayListObj();
                    matPacket->setDisplayListObj(matPacket->mpDisplayListObj);
                } else {
                    J3DDisplayListObj* dlobj = materialNode->getSharedDisplayListObj();
                    s32 ret = dlobj->single_To_Double();
                    if (ret != kJ3DError_Success) {
                        return ret;
                    }

                    matPacket->setDisplayListObj(dlobj);
                }
            } else if (mdlFlags & J3DMdlFlag_UseSharedDL) {
                if (mdlFlags & J3DMdlFlag_UseSingleDL) {
                    materialNode->newSingleSharedDisplayList(materialNode->countDLSize());

                    J3DDisplayListObj* dlobj = materialNode->getSharedDisplayListObj();
                    matPacket->setDisplayListObj(dlobj);
                } else {
                    materialNode->newSharedDisplayList(materialNode->countDLSize());

                    J3DDisplayListObj* dlobj = materialNode->getSharedDisplayListObj();
                    dlobj->single_To_Double();

                    matPacket->setDisplayListObj(dlobj);
                }
            } else {
                if (mdlFlags & J3DMdlFlag_UseSingleDL) {
                    matPacket->newSingleDisplayList(materialNode->countDLSize());
                } else {
                    matPacket->newDisplayList(materialNode->countDLSize());
                }
            }
        }
    }

    return kJ3DError_Success;
}

void J3DModel::lock() {
    int matNum = mModelData->getMaterialNum();
    for (int i = 0; i < matNum; i++) {
        mMatPacket[i].lock();
    }
}

void J3DModel::unlock() {
    int matNum = mModelData->getMaterialNum();
    for (int i = 0; i < matNum; i++) {
        mMatPacket[i].unlock();
    }
}

void J3DModel::makeDL() {
    j3dSys.setModel(this);
    j3dSys.setTexture(mModelData->getTexture());

    u16 matNum = mModelData->getMaterialNum();
    for (u16 i = 0; i < matNum; i++) {
        j3dSys.setMatPacket(&mMatPacket[i]);
        mModelData->getMaterialNodePointer(i)->makeDisplayList();
    }
}

void J3DModel::calcMaterial() {
    j3dSys.setModel(this);

    if (checkFlag(J3DMdlFlag_SkinPosCpu)) {
        j3dSys.onFlag(4);
    } else {
        j3dSys.offFlag(4);
    }

    if (checkFlag(J3DMdlFlag_SkinNrmCpu)) {
        j3dSys.onFlag(8);
    } else {
        j3dSys.offFlag(8);
    }

    mModelData->syncJ3DSysFlags();
    j3dSys.setTexture(mModelData->getTexture());

    u16 matNum = mModelData->getMaterialNum();
    for (u16 i = 0; i < matNum; i++) {
        j3dSys.setMatPacket(&mMatPacket[i]);

        J3DMaterial* material = mModelData->getMaterialNodePointer(i);
        if (material->getMaterialAnm() != NULL) {
            material->getMaterialAnm()->calc(material);
        }

        material->calc(getAnmMtx(material->getJoint()->getJntNo()));
    }
}

void J3DModel::calcDiffTexMtx() {
    j3dSys.setModel(this);

    u16 matNum = mModelData->getMaterialNum();
    for (u16 i = 0; i < matNum; i++) {
        j3dSys.setMatPacket(&mMatPacket[i]);
        J3DMaterial* materialNode = mModelData->getMaterialNodePointer(i);
        materialNode->calcDiffTexMtx(getAnmMtx(materialNode->getJoint()->getJntNo()));
    }

    u16 shapeNum = getModelData()->getShapeNum();
    for (u16 i = 0; i < shapeNum; i++) {
        J3DShapePacket* shapePacket = getShapePacket(i);
        J3DTexGenBlock* texGenBlock = mModelData->getShapeNodePointer(i)->getMaterial()->getTexGenBlock();

        for (u16 j = 0; (int)j < 8; j++) {
            J3DTexMtx* texMtxNode = texGenBlock->getTexMtx(j);
            J3DTexMtxObj* texMtxObj = shapePacket->getTexMtxObj();
            if (texMtxNode != NULL && texMtxObj != NULL) {
                texMtxObj->setMtx(j, texMtxNode->getMtx());
            }
        }
    }
}

#if TARGET_PC
#include <typeinfo>
extern "C++" void pc_print_current_stack(const char* why);
#endif

void J3DModel::calcAnmMtx() {
    j3dSys.setModel(this);
#if TARGET_PC
    // SMGPC_DEBUG_MTX=1: show who drives a model whose base matrix isn't finite (first few models).
    static const bool sDebugMtx = getenv("SMGPC_DEBUG_MTX") != nullptr;
    static int sReports = 0;
    if (sDebugMtx && sReports < 6 && getModelData()->getDrawMtxNum() > 1 && !(mBaseTransformMtx[0][3] == mBaseTransformMtx[0][3])) {
        sReports++;
        fprintf(stderr, "[pc] J3DModel %p (%u draw mtx) has a NaN base matrix\n", (void*)this,
                (u32)getModelData()->getDrawMtxNum());
        pc_print_current_stack("NaN base matrix");
    }
#endif

    if (checkFlag(J3DMdlFlag_UseDefaultJ3D)) {
        getModelData()->getJointTree().calc(mMtxBuffer, j3dDefaultScale, j3dDefaultMtx);
    } else {
        getModelData()->getJointTree().calc(mMtxBuffer, mBaseScale, mBaseTransformMtx);
    }
#if TARGET_PC
    static int sJointReports = 0;
    if (sDebugMtx && sJointReports < 8) {
        u16 jointNum = getModelData()->getJointNum();
        for (u16 i = 0; i < jointNum; i++) {
            MtxPtr m = mMtxBuffer->getAnmMtx(i);
            if (!(m[0][0] == m[0][0] && m[0][3] == m[0][3] && m[1][3] == m[1][3] && m[2][3] == m[2][3])) {
                sJointReports++;
                J3DJoint* joint = getModelData()->getJointNodePointer(i);
                const J3DTransformInfo& t = joint->getTransformInfo();
                fprintf(stderr,
                        "[pc] J3DModel %p: joint %u/%u NaN (mtxCalc %p %s, base calc %p) S %g %g %g R %d %d %d T %g %g %g "
                        "baseS %g %g %g baseT %g %g %g\n",
                        (void*)this, i, jointNum, (void*)joint->getMtxCalc(), joint->getMtxCalc() ? typeid(*joint->getMtxCalc()).name() : "-",
                        (void*)getModelData()->getJointTree().getBasicMtxCalc(), t.mScale.x, t.mScale.y, t.mScale.z,
                        t.mRotation.x, t.mRotation.y, t.mRotation.z, t.mTranslate.x, t.mTranslate.y, t.mTranslate.z,
                        mBaseScale.x, mBaseScale.y, mBaseScale.z, mBaseTransformMtx[0][3], mBaseTransformMtx[1][3],
                        mBaseTransformMtx[2][3]);
                if (sJointReports <= 2) {
                    pc_print_current_stack("NaN joint matrix");
                }
                break;
            }
        }
    }
#endif
}

void J3DModel::calcWeightEnvelopeMtx() {
    if (getModelData()->getWEvlpMtxNum() != 0 && !checkFlag(J3DMdlFlag_EnableLOD)) {
        if (!getModelData()->checkFlag(0x100)) {
            mMtxBuffer->calcWeightEnvelopeMtx();
        }
    }
}

void J3DModel::update() {
    calc();
    entry();
}

void J3DModel::calc() {
    j3dSys.setModel(this);

    if (checkFlag(J3DMdlFlag_SkinPosCpu)) {
        j3dSys.onFlag(J3DSysFlag_SkinPosCpu);
    } else {
        j3dSys.offFlag(J3DSysFlag_SkinPosCpu);
    }

    if (checkFlag(J3DMdlFlag_SkinNrmCpu)) {
        j3dSys.onFlag(J3DSysFlag_SkinNrmCpu);
    } else {
        j3dSys.offFlag(J3DSysFlag_SkinNrmCpu);
    }

    mModelData->syncJ3DSysFlags();
    mVertexBuffer.setCurrentVtxPos(mVertexBuffer.mVtxPosArray[0]);
    mVertexBuffer.setCurrentVtxNrm(mVertexBuffer.mVtxNrmArray[0]);
    mVertexBuffer.setCurrentVtxCol(mVertexBuffer.mVtxColArray[0]);

    if (mUnkCalc2 != NULL) {
        mUnkCalc2->calc(mModelData);
    }

    if (mDeformData != NULL) {
        mDeformData->deform(this);
    }

    if (mVtxColorCalc != NULL) {
        mVtxColorCalc->calc(this);
    }

    if (mUnkCalc1 != NULL) {
        mUnkCalc1->calc(this);
    }

    calcAnmMtx();
    calcWeightEnvelopeMtx();

    if (mSkinDeform != NULL) {
        mSkinDeform->deform(this);
    }

    if (mCalcCallBack != NULL) {
        mCalcCallBack(this, 0);
    }
}

void J3DModel::entry() {
    j3dSys.setModel(this);

    if (checkFlag(J3DMdlFlag_SkinPosCpu)) {
        j3dSys.onFlag(J3DSysFlag_SkinPosCpu);
    } else {
        j3dSys.offFlag(J3DSysFlag_SkinPosCpu);
    }

    if (checkFlag(J3DMdlFlag_SkinNrmCpu)) {
        j3dSys.onFlag(J3DSysFlag_SkinNrmCpu);
    } else {
        j3dSys.offFlag(J3DSysFlag_SkinNrmCpu);
    }

    mModelData->syncJ3DSysFlags();
    j3dSys.setTexture(mModelData->getTexture());

    for (u16 i = 0; i < mModelData->getJointNum(); i++) {
        J3DJoint* joint = mModelData->getJointNodePointer(i);
        if (joint->getMesh() != NULL) {
            joint->entryIn();
        }
    }
}

void J3DModel::viewCalc() {
    mMtxBuffer->swapDrawMtx();
    mMtxBuffer->swapNrmMtx();

    if (getModelData()->checkFlag(0x10)) {
        if (getMtxCalcMode() == 2) {
            J3DCalcViewBaseMtx(j3dSys.getViewMtx(), mBaseScale, mBaseTransformMtx, (MtxPtr)&mInternalView);
        }
    } else if (isCpuSkinningOn()) {
        if (getMtxCalcMode() == 2) {
            J3DCalcViewBaseMtx(j3dSys.getViewMtx(), mBaseScale, mBaseTransformMtx, (MtxPtr)&mInternalView);
        }
    } else if (checkFlag(J3DMdlFlag_SkinPosCpu)) {
        mMtxBuffer->calcDrawMtx(getMtxCalcMode(), mBaseScale, mBaseTransformMtx);
        calcNrmMtx();
        calcBumpMtx();
        DCStoreRangeNoSync(getDrawMtxPtr(), mModelData->getDrawMtxNum() * sizeof(Mtx));
        DCStoreRange(getNrmMtxPtr(), mModelData->getDrawMtxNum() * sizeof(Mtx33));
    } else if (checkFlag(J3DMdlFlag_SkinNrmCpu)) {
        mMtxBuffer->calcDrawMtx(getMtxCalcMode(), mBaseScale, mBaseTransformMtx);
        calcBBoardMtx();
        DCStoreRange(getDrawMtxPtr(), mModelData->getDrawMtxNum() * sizeof(Mtx));
    } else {
        mMtxBuffer->calcDrawMtx(getMtxCalcMode(), mBaseScale, mBaseTransformMtx);
        calcNrmMtx();
        calcBBoardMtx();
        calcBumpMtx();
        DCStoreRangeNoSync(getDrawMtxPtr(), mModelData->getDrawMtxNum() * sizeof(Mtx));
        DCStoreRange(getNrmMtxPtr(), mModelData->getDrawMtxNum() * sizeof(Mtx33));
    }

    prepareShapePackets();
}

void J3DModel::calcBumpMtx() {
    if (getModelData()->checkBumpFlag()) {
        u32 bumpMtxIdx = 0;
        u16 materialNum = getModelData()->getMaterialNum();

        for (u16 i = 0; i < materialNum; i++) {
            J3DMaterial* material = getModelData()->getMaterialNodePointer(i);
            if (material->getNBTScale()->mbHasScale == TRUE) {
                material->getShape()->calcNBTScale(*material->getNBTScale()->getScale(), getNrmMtxPtr(), getBumpMtxPtr(bumpMtxIdx));
                DCStoreRange(getBumpMtxPtr(bumpMtxIdx), mModelData->getDrawMtxNum() * sizeof(Mtx33));
                bumpMtxIdx++;
            }
        }
    }
}

void J3DModel::calcBBoardMtx() {
    if (getModelData()->checkBBoardFlag()) {
        mMtxBuffer->calcBBoardMtx();
    }
}

void J3DModel::prepareShapePackets() {
    u16 shapeNum = mModelData->getShapeNum();

    for (u16 i = 0; i < shapeNum; i++) {
        J3DShape* shapeNode = mModelData->getShapeNodePointer(i);
        J3DShapePacket* shapePacket = &mShapePacket[i];
        shapePacket->setMtxBuffer(mMtxBuffer);

        if (getMtxCalcMode() == 2) {
            shapePacket->setBaseMtxPtr(&mInternalView);
        } else {
            shapePacket->setBaseMtxPtr((Mtx*)j3dSys.getViewMtx());
        }
    }
}

Mtx33* J3DModel::getNrmMtxPtr() {
    return mMtxBuffer->getNrmMtxPtr();
}

Mtx33* J3DModel::getBumpMtxPtr(int idx) {
    return mMtxBuffer->getBumpMtxPtr(idx);
}

inline void J3DModel::calcNrmMtx() {
    mMtxBuffer->calcNrmMtx();
}
