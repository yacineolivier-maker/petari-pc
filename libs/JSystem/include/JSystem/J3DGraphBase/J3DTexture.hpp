#pragma once

#include "JSystem/J3DGraphBase/J3DTevs.hpp"
#include "JSystem/JUtility/JUTTexture.hpp"
#include <stdint.h>

class J3DTexture {
private:
    /* 0x0 */ u16 mNum;
    /* 0x2 */ u16 unk_0x2;
    /* 0x4 */ ResTIMG* mpRes;
#if TARGET_PC
    // Aurora can't take texture addresses through display lists (host pointers don't fit the
    // 24-bit BP address field), so every texture gets a persistent GX texture object that
    // J3DTevBlock::loadTexture binds when the material DL is drawn. The image/palette pointers
    // are kept separately because the 32-bit ResTIMG offsets can't span a 64-bit address space.
    GXTexObj* mpTexObj;
    GXTlutObj* mpTlutObj;
    u8** mpImgDataPtr;
    u8** mpTlutDataPtr;
#endif

public:
#if TARGET_PC
    J3DTexture(u16 num, ResTIMG* res);
    virtual ~J3DTexture();
    void loadGXTexObj(u16 idx);
    u8* getImgDataPtr(u16 idx) const {
        return mpImgDataPtr[idx];
    }
    u8* getTlutDataPtr(u16 idx) const {
        return mpTlutDataPtr[idx];
    }
#else
    J3DTexture(u16 num, ResTIMG* res) : mNum(num), unk_0x2(0), mpRes(res) {
    }
#endif

    void loadGX(u16, GXTexMapID) const;
    void entryNum(u16);
    void addResTIMG(u16, ResTIMG const*);
#if !TARGET_PC
    virtual ~J3DTexture() {
    }
#endif

    u16 getNum() const {
        return mNum;
    }

    ResTIMG* getResTIMG(u16 index) const {
        return &mpRes[index];
    }

    void setResTIMG(u16 index, const ResTIMG& timg) {
        mpRes[index] = timg;
        mpRes[index].mImageDataOffset = ((mpRes[index].mImageDataOffset + (uintptr_t)&timg - (uintptr_t)(mpRes + index)));
        mpRes[index].mPaletteDataOffset = ((mpRes[index].mPaletteDataOffset + (uintptr_t)&timg - (uintptr_t)(mpRes + index)));
#if TARGET_PC
        // The offsets above wrap when timg is more than 4 GB away; keep real pointers too and
        // rebuild the GX texture object (offsets are signed, see loadTexNo).
        mpImgDataPtr[index] = (u8*)&timg + (s32)timg.mImageDataOffset;
        mpTlutDataPtr[index] = (u8*)&timg + (s32)timg.mPaletteDataOffset;
        loadGXTexObj(index);
#endif
    }
};

extern J3DTexMtxInfo const j3dDefaultTexMtxInfo;

class J3DTexMtx {
public:
    J3DTexMtx() {
        mTexMtxInfo = j3dDefaultTexMtxInfo;
    }

    J3DTexMtx(const J3DTexMtxInfo& info) {
        mTexMtxInfo = info;
    }

    void load(u32) const;
    void calc(const Mtx);
    void calcTexMtx(const Mtx);
    void calcPostTexMtx(const Mtx);
    inline void loadTexMtx(u32) const;
    inline void loadPostTexMtx(u32) const;

    J3DTexMtxInfo& getTexMtxInfo() {
        return mTexMtxInfo;
    }
    Mtx& getMtx() {
        return mMtx;
    }
    void setEffectMtx(Mtx effectMtx) {
        mTexMtxInfo.setEffectMtx(effectMtx);
    }

    /* 0x00 */ J3DTexMtxInfo mTexMtxInfo;

private:
    /* 0x64 */ Mtx mMtx;
};  // Size: 0x94
