// The window icon: the executable's icon resource (pc/smgpc.rc, when it was built with one) as
// RGBA8 for Aurora/SDL. Separate file: windows.h clashes with the Wii types.

#define NOMINMAX
#include <windows.h>

#include <cstdint>
#include <cstring>
#include <vector>

// Returns false when the executable has no icon. `rgba` receives size*size pixels.
bool pc_load_window_icon(std::vector<uint8_t>& rgba, uint32_t& size) {
    size = 64;
    HICON icon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON, int(size), int(size),
                                               LR_DEFAULTCOLOR));
    if (icon == nullptr) {
        return false;
    }
    ICONINFO info = {};
    bool ok = false;
    if (GetIconInfo(icon, &info) && info.hbmColor != nullptr) {
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = int(size);
        bmi.bmiHeader.biHeight = -int(size);  // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        std::vector<uint8_t> bgra(size_t(size) * size * 4);
        HDC dc = GetDC(nullptr);
        if (GetDIBits(dc, info.hbmColor, 0, size, bgra.data(), &bmi, DIB_RGB_COLORS) == int(size)) {
            rgba.resize(bgra.size());
            for (size_t i = 0; i < bgra.size(); i += 4) {
                rgba[i + 0] = bgra[i + 2];
                rgba[i + 1] = bgra[i + 1];
                rgba[i + 2] = bgra[i + 0];
                rgba[i + 3] = bgra[i + 3];
            }
            ok = true;
        }
        ReleaseDC(nullptr, dc);
    }
    if (info.hbmColor != nullptr) {
        DeleteObject(info.hbmColor);
    }
    if (info.hbmMask != nullptr) {
        DeleteObject(info.hbmMask);
    }
    DestroyIcon(icon);
    return ok;
}
