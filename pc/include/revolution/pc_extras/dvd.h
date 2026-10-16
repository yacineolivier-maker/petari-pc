#pragma once
// PC extras: Wii SDK declarations missing from Aurora's dolphin/dvd headers.
#include <dolphin/dvd.h>

#ifdef __cplusplus
extern "C" {
#endif
// Wii disc check (dvd.h). Needs a PC implementation (pc/src): report "disc present".
BOOL DVDCheckDiskAsync(DVDCommandBlock* block, DVDCBCallback callback);
#ifdef __cplusplus
}
#endif
