# Builds pc/res/smgpc.ico from the game's own Wii save banner icon (48x48 RGB5A3), which the game
# writes to the emulated NAND (banner.bin) when it creates its save. Needs Pillow.
# Usage: python tools/pc/make_icon.py <banner.bin> pc/res/smgpc.ico
import struct
import sys

from PIL import Image

ICON_OFFSET = 0x20 + 0x80 + 192 * 64 * 2  # header, title/subtitle, 192x64 banner


def rgb5a3(v):
    if v & 0x8000:
        return (((v >> 10) & 31) * 255 // 31, ((v >> 5) & 31) * 255 // 31, (v & 31) * 255 // 31, 255)
    return (((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17, (v & 15) * 17, ((v >> 12) & 7) * 255 // 7)


def decode(data, w, h):
    img = Image.new("RGBA", (w, h))
    px = img.load()
    pos = 0
    for by in range(0, h, 4):
        for bx in range(0, w, 4):
            for y in range(by, by + 4):
                for x in range(bx, bx + 4):
                    px[x, y] = rgb5a3(struct.unpack(">H", data[pos:pos + 2])[0])
                    pos += 2
    return img


banner = open(sys.argv[1], "rb").read()
assert len(banner) >= ICON_OFFSET + 48 * 48 * 2, "banner too small"
icon = decode(banner[ICON_OFFSET:ICON_OFFSET + 48 * 48 * 2], 48, 48)
big = icon.resize((256, 256), Image.LANCZOS)
big.save(sys.argv[2], sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
icon.save(sys.argv[2].replace(".ico", "_48.png"))
print("wrote", sys.argv[2])
