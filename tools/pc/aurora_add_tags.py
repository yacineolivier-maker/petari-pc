#!/usr/bin/env python3
"""Give Aurora's anonymous GX/VI/OS typedefs the struct/enum tags the Wii SDK headers use.

The decomp refers to types by their SDK tag names (_GXRenderModeObj, _GXTlut, ...), which Aurora's
headers declare as anonymous typedefs. This rewrites `typedef struct {...} GXFoo;` and
`typedef enum {...} GXFoo;` into `typedef struct _GXFoo {...} GXFoo;` in extern/aurora/include.
The result is kept as pc/patches/aurora/0002-sdk-tags.patch (run, then `git -C extern/aurora diff`).

Usage: python tools/pc/aurora_add_tags.py
"""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
INC = ROOT / "extern/aurora/include/dolphin"

OPEN = re.compile(r"typedef\s+(struct|enum|union)\s*\{")


def process(text: str) -> str:
    out = []
    pos = 0
    for m in OPEN.finditer(text):
        # Find the matching close brace and the typedef name after it.
        depth = 0
        i = m.end() - 1
        while i < len(text):
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        tail = re.match(r"\s*([A-Za-z_][A-Za-z0-9_]*)\s*;", text[i + 1:])
        if not tail:
            continue
        name = tail.group(1)
        if not name.startswith(("GX", "VI", "OS", "DVD", "PAD", "GD")):
            continue
        out.append(text[pos:m.start()])
        out.append(f"typedef {m.group(1)} _{name} {{")
        pos = m.end()
    out.append(text[pos:])
    return "".join(out)


def main():
    changed = 0
    for path in INC.rglob("*.h"):
        text = path.read_text(encoding="utf-8")
        new = process(text)
        if new != text:
            path.write_text(new, encoding="utf-8", newline="\n")
            changed += 1
    print(f"tagged typedefs in {changed} headers")


if __name__ == "__main__":
    main()
