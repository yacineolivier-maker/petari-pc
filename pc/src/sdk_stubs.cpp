// Wii SDK functions with no PC counterpart, or not implemented yet. Each one either does what the
// game needs on PC or is an honest no-op; TODO(pc) marks lost behaviour.

#include <revolution/ai.h>
#include <revolution/dsp.h>
#include <revolution/dvd.h>
#include <revolution/gx.h>
#include <revolution/mem/allocator.h>
#include <revolution/os.h>
#include <revolution/pc_extras/hbm.h>
#include <revolution/sc.h>
#include <revolution/tpl.h>
#include <revolution/vi.h>
#include <revolution/wenc.h>
#include <revolution/nwc24.h>
#include <revolution/vf.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#include "pc/crash_handler.h"

// The DSP task list globals are declared with C++ linkage by revolution/dsp.h.
DSPTaskInfo* __DSP_first_task = nullptr;
DSPTaskInfo* __DSP_curr_task = nullptr;

extern "C" {

// --- Audio interface / DSP -------------------------------------------------------------------
// The DSP and the AI DMA are replaced by the HLE mixer + SDL3 output in pc/src/audio, which drives
// JASDriver::updateDSP() itself; these are never used for sound. DSPCheckMailToDSP must report
// "mailbox empty" or osdsp_task's busy-wait loops never end.
void AIInit(u8*) {}
void AIInitDMA(uintptr_t, u32) {}
static AIDCallback sAIDMACallback = nullptr;
AIDCallback AIRegisterDMACallback(AIDCallback callback) {
    AIDCallback old = sAIDMACallback;
    sAIDMACallback = callback;
    return old;
}
void AISetDSPSampleRate(u32) {}
void AIStartDMA(void) {}
void AIStopDMA(void) {}

void DSPInit(void) {}
void DSPAssertInt(void) {}
u32 DSPCheckMailFromDSP(void) { return 0; }
u32 DSPCheckMailToDSP(void) { return 0; }
u32 DSPReadMailFromDSP(void) { return 0; }
void DSPSendMailToDSP(u32) {}
void __DSP_boot_task(DSPTaskInfo*) {}
void __DSP_exec_task(DSPTaskInfo*, DSPTaskInfo*) {}
void __DSP_remove_task(DSPTaskInfo*) {}

// Aurora's ARAM buffer starts at offset 0 (JKRAram/JASHeap treat ARAM addresses as offsets).
u32 ARGetBaseAddress(void) { return 0x4000; }

// --- MEM allocators (nw4r layouts allocate through these) -------------------------------------
void* MEMAllocFromAllocator(MEMAllocator* allocator, u32 size) {
    return allocator->pFunc->pfAlloc(allocator, size);
}
void MEMFreeToAllocator(MEMAllocator* allocator, void* block) {
    allocator->pFunc->pfFree(allocator, block);
}

// --- OS / system -------------------------------------------------------------------------------
void OSRegisterVersion(const char*) {}
static OSPowerCallback sPowerCallback = nullptr;
OSPowerCallback OSSetPowerCallback(OSPowerCallback callback) {
    OSPowerCallback old = sPowerCallback;
    sPowerCallback = callback;
    return old;
}
// Leaving the game (Wii Menu, reset, power off) quits the PC game.
void OSRestart(u32) {
    pc_print_current_stack("OSRestart: the game asked to leave");
    std::exit(0);
}
void OSReturnToMenu(void) {
    pc_print_current_stack("OSReturnToMenu: the game asked to leave");
    std::exit(0);
}
void OSRebootSystem(void) {
    pc_print_current_stack("OSRebootSystem: the game asked to leave");
    std::exit(0);
}
void OSShutdownSystem(void) {
    pc_print_current_stack("OSShutdownSystem: the game asked to leave");
    std::exit(0);
}
void OSSleepTicks(OSTime ticks) {
    std::this_thread::sleep_for(std::chrono::microseconds(OSTicksToMicroseconds(ticks)));
}

u8 SCGetEuRgb60Mode(void) { return 1; }

BOOL VIEnableDimming(BOOL) { return FALSE; }
void VISetTrapFilter(VIBool) {}

// The disc is always there.
BOOL DVDCheckDiskAsync(DVDCommandBlock* block, DVDCBCallback callback) {
    if (callback != nullptr) {
        callback(1, block);
    }
    return TRUE;
}

// --- GX ----------------------------------------------------------------------------------------
// TODO(pc): EFB peeks/pokes are not available from Aurora (BrightObj tests the sun's visibility
// against the map collision instead).
void GXPeekARGB(u16, u16, u32* color) {
    if (color != nullptr) {
        *color = 0;
    }
}
void GXPokeAlphaRead(GXAlphaReadMode) {}

// GXSetZScaleOffset: the Wii scales and offsets the viewport depth range (__GXSetViewport:
// zScale = scale * 0xFFFFFF + 1, zOffset = offset * 0xFFFFFF, depth = [near, far] * zScale + zOffset)
// until the next GXSetZScaleOffset, viewport changes included. FallOutFieldDraw draws its fill at
// the far plane with (0, 1), Mario's silhouette pass pushes him back with (1, 0.00001).
static f32 sZScale = 1.0f;
static f32 sZOffset = 0.0f;

#undef GXSetViewport

// Rewrites the XF viewport registers like Aurora's GXSetViewportJitter, with the scaled depth range
// clamped to [0, 1] (WebGPU rejects viewport depths outside it).
static void LoadScaledViewport() {
    constexpr f32 kZMax = 16777215.0f;
    f32 vp[6];
    GXGetViewportv(vp);
    const f32 zScale = sZScale * kZMax + 1.0f;
    const f32 zOffset = sZOffset * kZMax;
    const f32 zNear = std::clamp((vp[4] * zScale + zOffset) / kZMax, 0.0f, 1.0f);
    const f32 zFar = std::clamp((vp[5] * zScale + zOffset) / kZMax, 0.0f, 1.0f);
    GXCmd1u8(0x10);
    GXCmd1u32(0x0005101A);
    GXCmd1f32(vp[2] / 2.0f);
    GXCmd1f32(-vp[3] / 2.0f);
    GXCmd1f32((zFar - zNear) * kZMax);
    GXCmd1f32(340.0f + vp[0] + vp[2] / 2.0f);
    GXCmd1f32(340.0f + vp[1] + vp[3] / 2.0f);
    GXCmd1f32(zFar * kZMax);
}

void GXSetZScaleOffset(f32 scale, f32 offset) {
    sZScale = scale;
    sZOffset = offset;
    LoadScaledViewport();
}

void pc_GXSetViewport(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz) {
    GXSetViewport(left, top, wd, ht, nearz, farz);
    if (sZScale != 1.0f || sZOffset != 0.0f) {
        LoadScaledViewport();
    }
}
void GXSetScissorBoxOffset(s32, s32) {}
void GXGetTexObjAll(const GXTexObj* obj, void** image, u16* width, u16* height, GXTexFmt* format, GXTexWrapMode* wrapS,
                    GXTexWrapMode* wrapT, GXBool* mipmap) {
    // Built from Aurora's single getters. LayoutManager::replaceIndDummyTexture needs it: it makes a
    // layout TexMap out of the screen copy texture (its image pointer is what GXCopyTex wrote to).
    if (image) *image = GXGetTexObjData(obj);
    if (width) *width = GXGetTexObjWidth(obj);
    if (height) *height = GXGetTexObjHeight(obj);
    if (format) *format = GXGetTexObjFmt(obj);
    if (wrapS) *wrapS = GXGetTexObjWrapS(obj);
    if (wrapT) *wrapT = GXGetTexObjWrapT(obj);
    if (mipmap) *mipmap = GXGetTexObjMipMap(obj);
}
// Aurora has no LOD getters: linear filtering without mipmaps, what the screen copy texture uses.
void GXGetTexObjLODAll(const GXTexObj*, GXTexFilter* minFilt, GXTexFilter* magFilt, f32* minLod, f32* maxLod,
                       f32* lodBias, GXBool* biasClamp, GXBool* edgeLod, GXAnisotropy* maxAniso) {
    if (minFilt) *minFilt = GX_LINEAR;
    if (magFilt) *magFilt = GX_LINEAR;
    if (minLod) *minLod = 0.0f;
    if (maxLod) *maxLod = 0.0f;
    if (lodBias) *lodBias = 0.0f;
    if (biasClamp) *biasClamp = GX_FALSE;
    if (edgeLod) *edgeLod = GX_FALSE;
    if (maxAniso) *maxAniso = GX_ANISO_1;
}

// --- TPL texture palettes ----------------------------------------------------------------------
// TPLBind relocates 32-bit offsets into pointers in place, impossible with 64-bit pointers.
// nw4r layouts read their .tpl textures directly (TexMap::ReplaceImage in lyt_texMap.cpp), so
// these are only reached by other TPL users (none known).
void TPLBind(TPLPalettePtr) {
    std::fprintf(stderr, "[pc] TPLBind called (not supported)\n");
}
TPLDescriptorPtr TPLGet(TPLPalettePtr, u32) { return nullptr; }

// --- Wii Remote speaker audio encoding (the speaker is not emulated) ---------------------------
s32 WENCGetEncodeData(WENCInfo*, u32, const s16*, s32, u8*) { return 0; }

// --- HOME button menu --------------------------------------------------------------------------
// TODO(pc): the Wii's HOME menu (return to Wii Menu, reset, Wii Remote settings) is not
// emulated. HBMCalc never selects anything, so the menu closes as soon as the game opens it.
void HBMCreate(const HBMDataInfo*) {}
void HBMDelete(void) {}
void HBMInit(void) {}
HBMSelectBtnNum HBMCalc(const HBMControllerData*) { return HBM_SELECT_HOMEBTN; }
void HBMDraw(void) {}
HBMSelectBtnNum HBMGetSelectBtnNum(void) { return HBM_SELECT_HOMEBTN; }
void HBMSetAdjustFlag(BOOL) {}
void HBMStartBlackOut(void) {}

// --- WiiConnect24 (Luigi's letters to the Wii Message Board) and the VF file system -----------
// No Wii Message Board on PC: the library reports "not supported" and the game skips the mail.
NWC24Err NWC24OpenLib(void*) { return NWC24_ERR_NOT_SUPPORTED; }
NWC24Err NWC24CloseLib(void) { return NWC24_OK; }
s32 NWC24GetErrorCode(void) { return 0; }
NWC24Err NWC24GetMyUserId(NWC24UserId*) { return NWC24_ERR_NOT_SUPPORTED; }
NWC24Err NWC24InitMsgObj(NWC24MsgObj*, NWC24MsgType) { return NWC24_ERR_NOT_SUPPORTED; }
NWC24Err NWC24SetMsgToId(NWC24MsgObj*, NWC24UserId) { return NWC24_ERR_NOT_SUPPORTED; }
NWC24Err NWC24SetMsgText(NWC24MsgObj*, const char*, u32, NWC24Charset, NWC24Encoding) { return NWC24_ERR_NOT_SUPPORTED; }
NWC24Err NWC24SetMsgAltName(NWC24MsgObj*, const u16*, u32) { return NWC24_ERR_NOT_SUPPORTED; }
NWC24Err NWC24SetMsgAttached(NWC24MsgObj*, const char*, u32, NWC24MIMEType) { return NWC24_ERR_NOT_SUPPORTED; }
NWC24Err NWC24SetMsgLedPattern(NWC24MsgObj*, u16) { return NWC24_ERR_NOT_SUPPORTED; }
NWC24Err NWC24SetMsgMBNoReply(NWC24MsgObj*, BOOL) { return NWC24_ERR_NOT_SUPPORTED; }
NWC24Err NWC24GetMsgSize(const NWC24MsgObj*, u32* size) {
    if (size != nullptr) {
        *size = 0;
    }
    return NWC24_ERR_NOT_SUPPORTED;
}
NWC24Err NWC24CommitMsg(NWC24MsgObj*) { return NWC24_ERR_NOT_SUPPORTED; }
void VFInitEx(void*, u32) {}

}  // extern "C"
