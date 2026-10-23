"""Writes the asset headers listed under `extract:` in config/RMGE01/config.yml (BCSV tables and the
error message archive embedded in main.dol), in the same form as the decomp build's
build/RMGE01/include/assets/*.hpp, so the PC build doesn't need the decomp toolchain. The data comes
from the user's own main.dol at configure time and is never committed.

Usage: python tools/pc/gen_assets.py <main.dol> <config.yml> <symbols.txt> <out include dir>
"""

import re
import sys
from pathlib import Path

from gen_dol_data import read_dol, read_va

PROLOGUE = """
#ifndef ATTRIBUTE_ALIGN
#if defined(__MWERKS__) || defined(__GNUC__)
#define ATTRIBUTE_ALIGN(num) __attribute__((aligned(num)))
#elif defined(_MSC_VER) || defined(__INTELLISENSE__)
#define ATTRIBUTE_ALIGN(num)
#else
#error unknown compiler
#endif
#endif
"""


def read_extract_list(config_path):
    entries, cur, in_extract = [], None, False
    for line in Path(config_path).read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if not line.startswith((" ", "-")):
            in_extract = line.startswith("extract:")
            continue
        if not in_extract:
            continue
        m = re.match(r"\s*(-\s*)?(\w+):\s*(\S+)", line)
        if not m:
            continue
        if m.group(1):
            cur = {}
            entries.append(cur)
        cur[m.group(2)] = m.group(3)
    return entries


def main():
    dol_path, config_path, symbols_path, out_dir = sys.argv[1:5]
    dol, sections = read_dol(dol_path)
    symbols = Path(symbols_path).read_text(encoding="utf-8")
    for entry in read_extract_list(config_path):
        name = entry["symbol"]
        m = re.search(rf"^{re.escape(name)} = (\.\w+):0x([0-9A-Fa-f]+);.*?size:0x([0-9A-Fa-f]+)"
                      r"(?:.*?align:(\d+))?", symbols, re.M)
        if not m:
            raise SystemExit(f"gen_assets: {name} not found in {symbols_path}")
        section, va, size = m.group(1), int(m.group(2), 16), int(m.group(3), 16)
        align = int(m.group(4) or 4)
        blob = read_va(dol, sections, va, size)
        out = [PROLOGUE,
               f"// {name} (size: {size:#X}, address: {va:#X}, section: {section})".replace("0X", "0x"),
               f"const unsigned char {name}[] ATTRIBUTE_ALIGN({align}) = {{"]
        for i in range(0, size, 16):
            out.append("    " + " ".join(f"0x{b:02X}," for b in blob[i:i + 16]))
        out.append("};\n")
        path = Path(out_dir) / entry["header"]
        path.parent.mkdir(parents=True, exist_ok=True)
        text = "\n".join(out)
        if not path.exists() or path.read_text(encoding="utf-8") != text:
            path.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main()
