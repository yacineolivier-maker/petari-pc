#pragma once
// PC extras: Wii SDK HOME Button Menu library entry points (homebuttonLib). The game's
// libs/RVL_SDK/include/revolution/hbm.h only has the types (the game loads the library from an RSO
// module). Need PC implementations (pc/src).
#include <revolution/hbm.h>

#ifdef __cplusplus
extern "C" {
#endif
void HBMCreate(const HBMDataInfo* pHBInfo);
void HBMDelete(void);
void HBMInit(void);
HBMSelectBtnNum HBMCalc(const HBMControllerData* pController);
void HBMDraw(void);
HBMSelectBtnNum HBMGetSelectBtnNum(void);
void HBMSetAdjustFlag(BOOL flag);
void HBMStartBlackOut(void);
#ifdef __cplusplus
}
#endif
