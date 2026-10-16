#pragma once
// PC shim (manual): Aurora's THP decoder plus the Wii SDK's THP types and the THPDraw helpers
// from libs/RVL_SDK/include/revolution/thp.h. Its decoder declarations are renamed so they don't
// clash with Aurora's (slightly different const-ness).
#include <dolphin/thp.h>
#define THPVideoDecode SMG_PC_THPVideoDecode_unused
#define THPAudioDecode SMG_PC_THPAudioDecode_unused
#define THPInit SMG_PC_THPInit_unused
#include "../../../libs/RVL_SDK/include/revolution/thp.h"
#undef THPVideoDecode
#undef THPAudioDecode
#undef THPInit
