// PC enhancement: sun shafts ("god rays") in the galaxies where the game shows a sun lens flare.
//
// Screen-space radial blur written with plain GX, like the game's own bloom (so Aurora needs no
// new shader): the bright parts of the frame are extracted on a small offscreen target, smeared
// towards the sun's screen position with two 8-tap passes (each tap is a texture matrix scaling
// about the sun), then added back to the frame. The sun position and visibility come from the
// game's LensFlareDirector (BrightSun's occlusion test), so the shafts fade with the flare.

#include "Game/Util/CameraUtil.hpp"
#include "Game/Util/DrawUtil.hpp"
#include "Game/Util/ScreenUtil.hpp"

#include <revolution/gx.h>
#include <dolphin/gx/GXAurora.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

float pc_settings_god_rays();

// Written by LensFlareDirector::controlFlare (src/Game/Screen/LensFlare.cpp).
float gPcSunScreenX = 0.0f;  // screen units (MR::getScreenWidth() wide, EFB height high)
float gPcSunScreenY = 0.0f;
float gPcSunBright = 0.0f;   // 0..1, 0 when the sun is hidden or outside a LensFlareArea

namespace {

// Copy textures are found by their destination pointer: these only serve as keys.
alignas(32) unsigned char sKeyFull[32];
alignas(32) unsigned char sKeyMask[32];
alignas(32) unsigned char sKeyBlur1[32];
alignas(32) unsigned char sKeyBlur2[32];

float sBright = 0.0f;  // smoothed

// Size of the intermediate passes (real pixels; the shafts are blurry anyway).
constexpr u32 kOffscreenWidth = 480;
constexpr u32 kOffscreenHeight = 270;

// Radial falloff around the sun (I8 texture), generated once.
constexpr int kGradientSize = 64;
alignas(32) unsigned char sGradient[kGradientSize * kGradientSize];

void InitGradient() {
    static bool sDone = false;
    if (sDone) {
        return;
    }
    sDone = true;
    for (int y = 0; y < kGradientSize; y++) {
        for (int x = 0; x < kGradientSize; x++) {
            const float dx = (x + 0.5f) / kGradientSize * 2.0f - 1.0f;
            const float dy = (y + 0.5f) / kGradientSize * 2.0f - 1.0f;
            float f = 1.0f - std::sqrt(dx * dx + dy * dy);
            f = f < 0.0f ? 0.0f : f * f;
            // I8 is stored in 8x4 tiles.
            const int tile = (y / 4) * (kGradientSize / 8) + (x / 8);
            sGradient[tile * 32 + (y % 4) * 8 + (x % 8)] = static_cast< unsigned char >(f * 255.0f);
        }
    }
}

void Copy(void* key, u16 width, u16 height) {
    GXSetTexCopySrc(0, 0, width, height);
    GXSetTexCopyDst(width, height, GX_TF_RGBA8, GX_FALSE);
    GXCopyTex(key, GX_FALSE);
    GXPixModeSync();
}

void Load(void* key, u16 width, u16 height, GXTexMapID map) {
    GXTexObj obj;
    GXInitTexObj(&obj, key, width, height, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GXInitTexObjLOD(&obj, GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
    GXLoadTexObj(&obj, map);
}

void Quad(f32 right, f32 bottom) {
    GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, 4);
    GXPosition3f32(0.0f, bottom, -1.0f);
    GXTexCoord2f32(0.0f, 1.0f);
    GXPosition3f32(0.0f, 0.0f, -1.0f);
    GXTexCoord2f32(0.0f, 0.0f);
    GXPosition3f32(right, bottom, -1.0f);
    GXTexCoord2f32(1.0f, 1.0f);
    GXPosition3f32(right, 0.0f, -1.0f);
    GXTexCoord2f32(1.0f, 0.0f);
    GXEnd();
}

void SetupDraw(f32 fbWidth, f32 fbHeight) {
    MR::reinitGX();
    Mtx44 projection;
    C_MTXOrtho(projection, 0.0f, fbHeight, 0.0f, fbWidth, -10000.0f, 10000.0f);
    GXSetProjection(projection, GX_ORTHOGRAPHIC);
    MR::loadViewMtxFor2DModel();
    GXClearVtxDesc();
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    Mtx identity;
    PSMTXIdentity(identity);
    GXLoadPosMtxImm(identity, GX_PNMTX0);
    GXSetCurrentMtx(GX_PNMTX0);
    GXSetCullMode(GX_CULL_NONE);
    GXSetClipMode(GX_CLIP_ENABLE);
    GXSetNumChans(1);
    GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    GXSetNumIndStages(0);
    GXSetFog(GX_FOG_NONE, 0.0f, 0.0f, 0.0f, 0.0f, GXColor{0, 0, 0, 0});
    GXSetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GXSetZCompLoc(GX_TRUE);
    GXSetColorUpdate(GX_TRUE);
    GXSetAlphaUpdate(GX_FALSE);
    GXSetDither(GX_FALSE);
}

// One texture through one TEV stage: texture * material color.
void SetupPlainTexture() {
    Mtx identity;
    PSMTXIdentity(identity);
    GXSetNumTexGens(1);
    GXLoadTexMtxImm(identity, GX_TEXMTX0, GX_MTX2x4);
    GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX0, GX_FALSE, GX_PTIDENTITY);
    GXSetNumTevStages(1);
    GXSetTevDirect(GX_TEVSTAGE0);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GXSetTevOp(GX_TEVSTAGE0, GX_MODULATE);
}

// 8 taps of the bound texture, each scaled about (u, v) by ratio^i, averaged.
void SetupRadialBlur(f32 u, f32 v, f32 ratio) {
    GXSetNumTexGens(8);
    GXSetNumTevStages(8);
    f32 scale = 1.0f;
    for (int i = 0; i < 8; i++) {
        Mtx m;
        PSMTXIdentity(m);
        m[0][0] = scale;
        m[1][1] = scale;
        m[0][3] = u * (1.0f - scale);
        m[1][3] = v * (1.0f - scale);
        const u32 texMtx = GX_TEXMTX0 + i * 3;
        GXLoadTexMtxImm(m, texMtx, GX_MTX2x4);
        GXSetTexCoordGen2(static_cast< GXTexCoordID >(GX_TEXCOORD0 + i), GX_TG_MTX2x4, GX_TG_TEX0, texMtx, GX_FALSE, GX_PTIDENTITY);

        const GXTevStageID stage = static_cast< GXTevStageID >(GX_TEVSTAGE0 + i);
        GXSetTevDirect(stage);
        GXSetTevOrder(stage, static_cast< GXTexCoordID >(GX_TEXCOORD0 + i), GX_TEXMAP0, GX_COLOR_NULL);
        GXSetTevKColorSel(stage, GX_TEV_KCSEL_1_8);
        GXSetTevKAlphaSel(stage, GX_TEV_KASEL_1_8);
        // out = (i == 0 ? 0 : prev) + tex * 1/8
        GXSetTevColorIn(stage, GX_CC_ZERO, GX_CC_TEXC, GX_CC_KONST, i == 0 ? GX_CC_ZERO : GX_CC_CPREV);
        GXSetTevColorOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        GXSetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);
        GXSetTevAlphaOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        scale *= ratio;
    }
}

}  // namespace

void pc_god_rays_draw() {
    const float strength = pc_settings_god_rays();
    if (strength <= 0.0f) {
        return;
    }
    // SMGPC_DEBUG_GODRAYS=1: log the sun every second; =2: also force the shafts on.
    static const int sDebug = std::getenv("SMGPC_DEBUG_GODRAYS") ? std::atoi(std::getenv("SMGPC_DEBUG_GODRAYS")) : 0;
    if (sDebug != 0) {
        static int sFrames = 0;
        if (sFrames++ % 60 == 0) {
            std::fprintf(stderr, "[pc] sun %.1f %.1f bright %.2f\n", gPcSunScreenX, gPcSunScreenY, gPcSunBright);
        }
        if (sDebug == 2) {
            gPcSunBright = 1.0f;
        }
    }
    sBright += (gPcSunBright - sBright) * 0.15f;
    if (sBright < 0.01f) {
        return;
    }

    const u16 width = static_cast< u16 >(MR::getFrameBufferWidth());
    const u16 height = static_cast< u16 >(MR::getFrameBufferHeight());
    const f32 sunU = gPcSunScreenX / static_cast< f32 >(MR::getScreenWidth());
    const f32 sunV = gPcSunScreenY / static_cast< f32 >(height);

    // The finished frame as a texture (the only copy of the full-resolution EFB). The passes then
    // run on a small offscreen target, so the EFB is left as it is.
    Copy(sKeyFull, width, height);
    GXCreateFrameBuffer(kOffscreenWidth, kOffscreenHeight);

    SetupDraw(width, height);
    GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);

    // Bright parts only, (color - threshold) * 4, faded with the distance to the sun, at quarter
    // size.
    SetupPlainTexture();
    Load(sKeyFull, width, height, GX_TEXMAP0);
    InitGradient();
    {
        GXTexObj gradient;
        GXInitTexObj(&gradient, sGradient, kGradientSize, kGradientSize, GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE);
        GXInitTexObjLOD(&gradient, GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
        GXLoadTexObj(&gradient, GX_TEXMAP1);
        // Screen UV -> gradient UV: a circle of radius 0.45 screen width around the sun.
        const f32 radius = 0.45f;
        // (corrected for windows that aren't 16:9, see CameraContext::getAspect)
        const f32 aspect = static_cast< f32 >(height) / static_cast< f32 >(MR::getScreenWidth()) * (16.0f / 9.0f) / MR::getAspect();
        Mtx m;
        PSMTXIdentity(m);
        m[0][0] = 0.5f / radius;
        m[1][1] = 0.5f * aspect / radius;
        m[0][3] = 0.5f - sunU * m[0][0];
        m[1][3] = 0.5f - sunV * m[1][1];
        GXLoadTexMtxImm(m, GX_TEXMTX1, GX_MTX2x4);
        GXSetNumTexGens(2);
        GXSetTexCoordGen2(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX1, GX_FALSE, GX_PTIDENTITY);
    }
    GXSetNumTevStages(2);
    GXSetTevKColor(GX_KCOLOR0, GXColor{200, 200, 200, 255});
    GXSetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K0);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_KONST, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_4, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevDirect(GX_TEVSTAGE1);
    GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP1, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_CPREV, GX_CC_TEXC, GX_CC_ZERO);  // prev * gradient
    GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
    GXSetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    Quad(width, height);
    Copy(sKeyMask, width, height);

    // Two radial blur passes: short taps, then taps 8 times longer.
    Load(sKeyMask, width, height, GX_TEXMAP0);
    SetupRadialBlur(sunU, sunV, 0.965f);
    Quad(width, height);
    Copy(sKeyBlur1, width, height);

    Load(sKeyBlur1, width, height, GX_TEXMAP0);
    SetupRadialBlur(sunU, sunV, std::pow(0.965f, 8.0f));
    Quad(width, height);
    Copy(sKeyBlur2, width, height);
    GXRestoreFrameBuffer();

    // Shafts added on top of the frame (warm white).
    SetupDraw(width, height);
    SetupPlainTexture();
    const f32 intensity = std::fmin(sBright * strength, 1.0f);
    GXSetChanMatColor(GX_COLOR0A0, GXColor{static_cast< u8 >(255 * intensity), static_cast< u8 >(235 * intensity),
                                           static_cast< u8 >(190 * intensity), 255});
    GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_CLEAR);
    Load(sKeyBlur2, width, height, GX_TEXMAP0);
    Quad(width, height);

    GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
    MR::reinitGX();
}
