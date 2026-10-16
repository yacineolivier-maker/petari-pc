#pragma once
// PC shim (manual): the Wii SDK DSP header itself (task struct names used by JAudio2's DSP code).
// The PC build replaces the DSP with an HLE mixer; Aurora's dolphin/dsp.h is not used.
#include "../../../libs/RVL_SDK/include/revolution/dsp.h"
