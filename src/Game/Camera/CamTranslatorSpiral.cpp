#include "Game/Camera/CamTranslatorSpiral.hpp"
#include "Game/Camera/CameraParamChunk.hpp"

void CamTranslatorSpiral::setParam(const CameraParamChunk* pChunk) {
    CameraGeneralParam* general = pChunk->mGeneralParam;

#if TARGET_PC
    // mNum1 packs two s16 (high half first on the big-endian Wii).
    s32 startTime = static_cast< s16 >(static_cast< u32 >(general->mNum1) >> 16);
    s32 endTime = static_cast< s16 >(general->mNum1 & 0xFFFF);
#else
    s32 startTime = reinterpret_cast< s16* >(&general->mNum1)[0];
    s32 endTime = reinterpret_cast< s16* >(&general->mNum1)[1];
#endif

    mCamera->setParam(general->mNum2, startTime, endTime, general->mWPoint.y, general->mAxis.y, general->mWPoint.z, general->mAxis.z,
                      general->mWPoint.x, general->mAxis.x);
}

Camera* CamTranslatorSpiral::getCamera() const {
    return mCamera;
}
