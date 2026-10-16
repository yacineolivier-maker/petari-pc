#include "nw4r/lyt/common.h"
#include "nw4r/lyt/texMap.h"
#include "nw4r/ut/resEndian.h"
#include <revolution/gx/GXEnum.h>
#include <revolution/gx/GXGet.h>
#include <revolution/gx/GXStruct.h>
#include <revolution/tpl.h>

namespace nw4r {
    namespace lyt {
        void TexMap::Get(_GXTexObj* pTexObj) const {
            if (detail::IsCITexelFormat(GetTexelFormat())) {
                u32 tlutName = GXGetTexObjTlut(pTexObj);
                GXInitTexObjCI(pTexObj, mImage, mWidth, mHeight, GXCITexFmt(GetTexelFormat()), GetWrapModeS(), GetWrapModeT(), IsMipMap(), tlutName);
            } else {
                GXInitTexObj(pTexObj, mImage, mWidth, mHeight, GetTexelFormat(), GetWrapModeS(), GetWrapModeT(), IsMipMap());
            }

            GXInitTexObjLOD(pTexObj, GetMinFilter(), GetMagFilter(), GetMinLOD(), GetMaxLOD(), GetLODBias(), IsBiasClampEnable(), IsEdgeLODEnable(),
                            GetAnisotropy());
        }

        void TexMap::Get(_GXTlutObj* pTlutObj) const {
            GXInitTlutObj(pTlutObj, GetPalette(), GetPaletteFormat(), GetPaletteEntryNum());
        }

        void TexMap::Set(const GXTexObj& texObj) {
            void* image;
            u16 width, height;
            GXTexFmt format;
            GXTexWrapMode wrapS, wrapT;
            GXBool mipmap;

            GXGetTexObjAll(&texObj, &image, &width, &height, &format, &wrapS, &wrapT, &mipmap);

            mImage = image;
            SetSize(width, height);
            mBits.textureFormat = format;
            SetWrapMode(wrapS, wrapT);
            SetMipMap(mipmap);

            GXTexFilter minFilter, magFilter;
            f32 minLOD, maxLOD, lodBias;
            GXBool biasCLampEnable, edgeLODEnable;
            GXAnisotropy aniso;
            GXGetTexObjLODAll(&texObj, &minFilter, &magFilter, &minLOD, &maxLOD, &lodBias, &biasCLampEnable, &edgeLODEnable, &aniso);

            SetFilter(minFilter, magFilter);
            SetLOD(minLOD, maxLOD);
            SetLODBias(lodBias);
            SetBiasClampEnable(biasCLampEnable);
            SetEdgeLODEnable(edgeLODEnable);
            mBits.anisotropy = aniso;
        }

        void TexMap::ReplaceImage(const TPLDescriptor* pTPLDesc) {
            const TPLHeader& header = *pTPLDesc->textureHeader;
            mImage = header.data;
            SetSize(header.width, header.height);
            SetTexelFormat(GXTexFmt(header.format));

            if (const TPLClutHeader* const pClut = pTPLDesc->CLUTHeader) {
                SetPalette(pClut->data);
                SetPaletteFormat(pClut->format);
                SetPaletteEntryNum(pClut->numEntries);
            } else {
                SetPalette(nullptr);
                SetPaletteFormat(GXTlutFmt(0));
                SetPaletteEntryNum(0);
            }
        }

        void TexMap::ReplaceImage(TPLPalette* p, u32 id) {
#if TARGET_PC
            // TPL files are big-endian and hold 32-bit offsets from the start of the file, which
            // TPLBind relocates in place into pointers on the Wii. That cannot work with 64-bit
            // pointers (TPLPalette/TPLDescriptor/TPLHeader have pointer fields), so the file is
            // never bound on PC and the offsets are followed here, read-only:
            //   palette:    u32 version, u32 numDescriptors, u32 descriptorArray
            //   descriptor: u32 textureHeader, u32 CLUTHeader (0 = none)
            //   header:     u16 height, u16 width, u32 format, u32 data, ...
            //   CLUT:       u16 numEntries, u8 unpacked, u8 pad, u32 format, u32 data
            u8* const pBase = reinterpret_cast< u8* >(p);
            const BE(u32)* const pPalette = reinterpret_cast< const BE(u32)* >(pBase);
            const u32 numDescriptors = pPalette[1];
            if (numDescriptors == 0) {
                return;
            }

            const BE(u32)* const pDesc = reinterpret_cast< const BE(u32)* >(pBase + pPalette[2] + (id % numDescriptors) * 8);
            u8* const pHeader = pBase + pDesc[0];
            const BE(u16)* const pHeader16 = reinterpret_cast< const BE(u16)* >(pHeader);
            const BE(u32)* const pHeader32 = reinterpret_cast< const BE(u32)* >(pHeader);
            mImage = pBase + pHeader32[2];
            SetSize(pHeader16[1], pHeader16[0]);
            SetTexelFormat(GXTexFmt(static_cast< u32 >(pHeader32[1])));

            const u32 clutOffs = pDesc[1];
            if (clutOffs != 0) {
                u8* const pClut = pBase + clutOffs;
                const BE(u32)* const pClut32 = reinterpret_cast< const BE(u32)* >(pClut);
                SetPalette(pBase + pClut32[2]);
                SetPaletteFormat(GXTlutFmt(static_cast< u32 >(pClut32[1])));
                SetPaletteEntryNum(*reinterpret_cast< const BE(u16)* >(pClut));
            } else {
                SetPalette(nullptr);
                SetPaletteFormat(GXTlutFmt(0));
                SetPaletteEntryNum(0);
            }
#else
            if (reinterpret_cast< u32 >(p->descriptorArray) < 0x80000000) {
                TPLBind(p);
            }

            ReplaceImage(TPLGet(p, id));
#endif
        }
    };  // namespace lyt
};  // namespace nw4r
