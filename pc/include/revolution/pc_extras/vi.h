#pragma once
// PC extras.
#include <dolphin/vi.h>

#ifdef __cplusplus
extern "C" {
#endif
// Wii screen-dimming (burn-in protection) counter; not in Aurora. Needs a PC implementation (pc/src).
u32 VIGetDimmingCount(void);
#ifdef __cplusplus
}
#endif

// Wii SDK VI additions (vitypes.h / vi3in1types.h / vifuncs.h / vi3in1.h) missing from Aurora.
#ifndef VI_TVMODE_EURGB60_PROG
#define VI_TVMODE_EURGB60_PROG ((VITVMode)VI_TVMODE(VI_EURGB60, VI_PROGRESSIVE))
#endif
#ifndef VI_TVMODE_MPAL_PROG
#define VI_TVMODE_MPAL_PROG ((VITVMode)VI_TVMODE(VI_MPAL, VI_PROGRESSIVE))
#endif
#ifndef VI_MAX_WIDTH_NTSC
#define VI_MAX_WIDTH_NTSC 720
#define VI_MAX_HEIGHT_NTSC 480
#define VI_MAX_WIDTH_PAL 720
#define VI_MAX_HEIGHT_PAL 574
#define VI_MAX_WIDTH_MPAL 720
#define VI_MAX_HEIGHT_MPAL 480
#define VI_MAX_WIDTH_EURGB60 VI_MAX_WIDTH_NTSC
#define VI_MAX_HEIGHT_EURGB60 VI_MAX_HEIGHT_NTSC
#endif
#ifndef VI_TRUE
typedef u8 VIBool;
#define VI_FALSE ((VIBool)0)
#define VI_TRUE ((VIBool)1)
#define VI_DISABLE ((VIBool)0)
#define VI_ENABLE ((VIBool)1)
#endif
#ifdef __cplusplus
extern "C" {
#endif
// Need PC implementations (pc/src): burn-in dimming on/off, deflicker ("trap") filter.
BOOL VIEnableDimming(BOOL enable);
void VISetTrapFilter(VIBool filter);
#ifdef __cplusplus
}
#endif
