// Xbox controller pictograms for the PC port: GDI renderer (no game headers here, windows.h clashes
// with the Wii types). See xbox_glyphs.cpp.
//
// The game shows its Wii buttons through the picture font (LayoutData/Font.arc PictureFont.brfnt,
// RGB5A3 glyph sheets, one 34x35 cell per glyph) with codes like '0' for the A button. After the
// font is loaded, the cells of the buttons that have an Xbox equivalent are redrawn with
// generated icons (GDI shapes and text, supersampled for antialiasing), following the PC mapping
// of pc/src/input/wiimote.cpp. Nothing from the game's data is copied.

#include "pc/xbox_glyphs.h"

#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

enum class Shape { Circle, Pill };

struct IconSpec {
    unsigned short code;
    Shape shape;
    const wchar_t* label;
    COLORREF labelColor;
    const wchar_t* fontName;
};

constexpr COLORREF kWhite = RGB(240, 240, 240);
constexpr COLORREF kGreen = RGB(110, 200, 40);
constexpr COLORREF kRed = RGB(232, 60, 50);
constexpr COLORREF kYellow = RGB(250, 190, 20);

// Picture font codes (see GalaxyInfoLayoutSetter / MR::addPictureFontCode): '0' A, '1' B trigger,
// '2' C, '3' Wii Remote (shake), '4' Nunchuk stick, '5' 1, 'G' -, 'H' +, 'I' Z.
const IconSpec kIcons[] = {
    {'0', Shape::Circle, L"A", kGreen, L"Segoe UI"},
    {'1', Shape::Pill, L"RT", kWhite, L"Segoe UI"},
    {'?', Shape::Pill, L"RT", kWhite, L"Segoe UI"},  // B trigger seen from above (e.g. "Retour")
    {'2', Shape::Pill, L"LB", kWhite, L"Segoe UI"},
    {'3', Shape::Circle, L"B", kRed, L"Segoe UI"},
    {'4', Shape::Circle, L"L", kWhite, L"Segoe UI"},
    {'5', Shape::Circle, L"Y", kYellow, L"Segoe UI"},
    {'G', Shape::Circle, L"⧉", kWhite, L"Segoe UI Symbol"},
    {'H', Shape::Circle, L"≡", kWhite, L"Segoe UI Symbol"},
    {'I', Shape::Pill, L"LT", kWhite, L"Segoe UI"},
};

constexpr int kSuper = 4;  // supersampling factor

// Renders one icon into straight-alpha RGBA (w x h).
std::vector<uint8_t> RenderIcon(const IconSpec& icon, int w, int h) {
    const int W = w * kSuper, H = h * kSuper;
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = W;
    bmi.bmiHeader.biHeight = -H;  // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bmp = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    std::vector<uint8_t> out(size_t(w) * h * 4, 0);
    if (bmp == nullptr || bits == nullptr) {
        DeleteDC(dc);
        return out;
    }
    HGDIOBJ oldBmp = SelectObject(dc, bmp);
    // Magenta background = transparent (exact color, never produced by the icon itself).
    const COLORREF kKey = RGB(255, 0, 255);
    HBRUSH keyBrush = CreateSolidBrush(kKey);
    RECT all = {0, 0, W, H};
    FillRect(dc, &all, keyBrush);
    DeleteObject(keyBrush);

    // Body: dark disc / pill with a light rim, centered, a bit inside the cell.
    const int margin = 2 * kSuper;
    const int size = std::min(W, H) - 2 * margin;
    RECT body;
    if (icon.shape == Shape::Circle) {
        body = {(W - size) / 2, (H - size) / 2, (W + size) / 2, (H + size) / 2};
    } else {
        const int bw = W - 2 * margin, bh = size * 3 / 4;
        body = {(W - bw) / 2, (H - bh) / 2, (W + bw) / 2, (H + bh) / 2};
    }
    HPEN rim = CreatePen(PS_SOLID, 2 * kSuper, RGB(200, 200, 205));
    HBRUSH fill = CreateSolidBrush(RGB(40, 42, 46));
    HGDIOBJ oldPen = SelectObject(dc, rim);
    HGDIOBJ oldBrush = SelectObject(dc, fill);
    if (icon.shape == Shape::Circle) {
        Ellipse(dc, body.left, body.top, body.right, body.bottom);
    } else {
        const int r = (body.bottom - body.top);
        RoundRect(dc, body.left, body.top, body.right, body.bottom, r, r);
    }
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(rim);
    DeleteObject(fill);

    // Label.
    const int textHeight = icon.shape == Shape::Circle ? size * 3 / 4 : (body.bottom - body.top) * 4 / 5;
    HFONT font = CreateFontW(-textHeight, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH, icon.fontName);
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, icon.labelColor);
    DrawTextW(dc, icon.label, -1, &body, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, oldFont);
    DeleteObject(font);
    GdiFlush();

    // Box-filter down to the cell size; the key color counts as transparent.
    const uint8_t* src = static_cast<const uint8_t*>(bits);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int r = 0, g = 0, b = 0, n = 0;
            for (int sy = 0; sy < kSuper; sy++) {
                for (int sx = 0; sx < kSuper; sx++) {
                    const uint8_t* p = src + (size_t((y * kSuper + sy)) * W + (x * kSuper + sx)) * 4;  // BGRA
                    if (p[2] == 255 && p[1] == 0 && p[0] == 255) {
                        continue;
                    }
                    b += p[0];
                    g += p[1];
                    r += p[2];
                    n++;
                }
            }
            uint8_t* o = &out[(size_t(y) * w + x) * 4];
            if (n > 0) {
                o[0] = uint8_t(r / n);
                o[1] = uint8_t(g / n);
                o[2] = uint8_t(b / n);
                o[3] = uint8_t(n * 255 / (kSuper * kSuper));
            }
        }
    }
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
    return out;
}

}  // namespace

int pc_xbox_icon_count() { return int(sizeof(kIcons) / sizeof(kIcons[0])); }

unsigned short pc_xbox_icon_code(int index) { return kIcons[index].code; }

void pc_xbox_icon_render(int index, int w, int h, unsigned char* rgba) {
    const std::vector<uint8_t> out = RenderIcon(kIcons[index], w, h);
    std::copy(out.begin(), out.end(), rgba);
}
