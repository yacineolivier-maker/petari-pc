#pragma once

#include "JSystem/J3DGraphBase/J3DTransform.hpp"
#include "helpers/endian.h"

class J3DJoint;
struct J3DJointBlock;

// JNT1 file data (big-endian, read in place).
struct J3DJointInitData {
    /* 0x00 */ BE(u16) mKind;
    /* 0x02 */ bool mScaleCompensate;
#if TARGET_PC
    // Big-endian file layout of J3DTransformInfo.
    struct {
        /* 0x00 */ BE(Vec) mScale;
        /* 0x0C */ BE(s16) mRotation[3];
        /* 0x12 */ u8 _12[2];
        /* 0x14 */ BE(Vec) mTranslate;
    } /* 0x04 */ mTransformInfo;
#else
    /* 0x04 */ J3DTransformInfo mTransformInfo;
#endif
    /* 0x24 */ BE(f32) mRadius;
    /* 0x28 */ BE(Vec) mMin;
    /* 0x34 */ BE(Vec) mMax;
};  // Size: 0x40

struct J3DJointFactory {
    J3DJointFactory(J3DJointBlock const&);
    J3DJoint* create(int);

    J3DJointInitData* mJointInitData;
    BE(u16)* mIndexTable;

    u16 getKind(int no) const {
        return mJointInitData[mIndexTable[no]].mKind;
    }
    u8 getScaleCompensate(int no) const {
        return mJointInitData[mIndexTable[no]].mScaleCompensate;
    }
#if TARGET_PC
    J3DTransformInfo getTransformInfo(int no) const {
        const J3DJointInitData& data = mJointInitData[mIndexTable[no]];
        J3DTransformInfo info;
        info.mScale = (Vec)data.mTransformInfo.mScale;
        info.mRotation.x = data.mTransformInfo.mRotation[0];
        info.mRotation.y = data.mTransformInfo.mRotation[1];
        info.mRotation.z = data.mTransformInfo.mRotation[2];
        info.mTranslate = (Vec)data.mTransformInfo.mTranslate;
        return info;
    }
    f32 getRadius(int no) const {
        return mJointInitData[mIndexTable[no]].mRadius;
    }
    Vec getMin(int no) const {
        return mJointInitData[mIndexTable[no]].mMin;
    }
    Vec getMax(int no) const {
        return mJointInitData[mIndexTable[no]].mMax;
    }
#else
    const J3DTransformInfo& getTransformInfo(int no) const {
        return mJointInitData[mIndexTable[no]].mTransformInfo;
    }
    f32 getRadius(int no) const {
        return mJointInitData[mIndexTable[no]].mRadius;
    }
    Vec& getMin(int no) const {
        return mJointInitData[mIndexTable[no]].mMin;
    }
    Vec& getMax(int no) const {
        return mJointInitData[mIndexTable[no]].mMax;
    }
#endif
};
