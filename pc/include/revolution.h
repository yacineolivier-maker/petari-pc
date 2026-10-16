#pragma once
// PC shim (manual): the original libs/RVL_SDK/include/revolution.h uses quoted includes, which
// would resolve next to it and bypass the shims in pc/include/revolution.
#include <revolution/ai.h>
#include <revolution/arc.h>
#include <revolution/dvd.h>
#include <revolution/fs.h>
#include <revolution/gd.h>
#include <revolution/gx.h>
#include <revolution/kpad.h>
#include <revolution/mtx.h>
#include <revolution/nand.h>
#include <revolution/os.h>
#include <revolution/vi.h>
#include <revolution/wenc.h>
#include <revolution/wpad.h>
