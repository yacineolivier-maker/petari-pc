#pragma once

namespace nw4r {
    namespace ut {
        class Font;
    }
}  // namespace nw4r

// Redraws the Wii button glyphs of the picture font as Xbox controller icons (SMGPC_WII_ICONS=1
// keeps the Wii ones). Call once after the font resource is loaded.
void pc_patch_picture_font(nw4r::ut::Font* font);

// Icon table (pc/src/ui/xbox_glyphs_gdi.cpp): picture font code and straight-alpha RGBA rendering.
int pc_xbox_icon_count();
unsigned short pc_xbox_icon_code(int index);
void pc_xbox_icon_render(int index, int w, int h, unsigned char* rgba);
