// Xbox controller pictograms for the PC port: patches the picture font (see xbox_glyphs_gdi.cpp for
// the icons and the mapping).
//
// The game shows its Wii buttons through the picture font (LayoutData/Font.arc PictureFont.brfnt,
// RGB5A3 glyph sheets, one 34x35 cell per glyph). After the font is loaded, the cells of the buttons
// that have an Xbox equivalent are redrawn with the generated icons. Nothing from the game's data
// is copied.

#include "pc/xbox_glyphs.h"

#include <nw4r/ut/Font.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

uint16_t ToRGB5A3(const uint8_t* c) {
    if (c[3] >= 0xE0) {
        return uint16_t(0x8000 | ((c[0] >> 3) << 10) | ((c[1] >> 3) << 5) | (c[2] >> 3));
    }
    return uint16_t(((c[3] >> 5) << 12) | ((c[0] >> 4) << 8) | ((c[1] >> 4) << 4) | (c[2] >> 4));
}

// Writes one texel of a big-endian RGB5A3 texture (4x4 tiles).
void PutTexel(uint8_t* tex, int texWidth, int x, int y, uint16_t v) {
    const int tilesPerRow = texWidth / 4;
    const int tile = (y / 4) * tilesPerRow + (x / 4);
    const int offset = tile * 32 + ((y % 4) * 4 + (x % 4)) * 2;
    tex[offset] = uint8_t(v >> 8);
    tex[offset + 1] = uint8_t(v);
}

}  // namespace

void pc_patch_picture_font(nw4r::ut::Font* font) {
    if (font == nullptr || std::getenv("SMGPC_WII_ICONS") != nullptr) {
        return;
    }
    int patched = 0;
    for (int i = 0; i < pc_xbox_icon_count(); i++) {
        const nw4r::ut::CharCode code = pc_xbox_icon_code(i);
        if (!font->HasGlyph(code)) {
            continue;
        }
        nw4r::ut::Glyph glyph;
        font->GetGlyph(&glyph, code);
        if (glyph.texFormat != GX_TF_RGB5A3 || glyph.pTexture == nullptr) {
            continue;
        }
        const int w = glyph.widths.glyphWidth;
        const int h = glyph.height;
        if (w <= 0 || h <= 0) {
            continue;
        }
        std::vector<uint8_t> rgba(size_t(w) * h * 4);
        pc_xbox_icon_render(i, w, h, rgba.data());
        uint8_t* tex = static_cast<uint8_t*>(glyph.pTexture);
        for (int y = 0; y < h && glyph.cellY + y < glyph.texHeight; y++) {
            for (int x = 0; x < w && glyph.cellX + x < glyph.texWidth; x++) {
                PutTexel(tex, glyph.texWidth, glyph.cellX + x, glyph.cellY + y, ToRGB5A3(&rgba[(size_t(y) * w + x) * 4]));
            }
        }
        patched++;
        // SMGPC_DUMP_PICTFONT=<prefix>: write the patched sheet (raw big-endian RGB5A3) for checking.
        if (const char* dump = std::getenv("SMGPC_DUMP_PICTFONT")) {
            char path[512];
            std::snprintf(path, sizeof(path), "%s_%c_%ux%u.raw", dump, char(code), glyph.texWidth, glyph.texHeight);
            if (FILE* f = std::fopen(path, "wb")) {
                std::fwrite(tex, 1, size_t(glyph.texWidth) * glyph.texHeight * 2, f);
                std::fclose(f);
            }
        }
    }
    std::fprintf(stderr, "[pc] picture font: %d Xbox button icons\n", patched);
}
