#!/usr/bin/env python3
"""Make forward declarations use the same class-key (class/struct) as the type's definition.

MSVC mangles `class X` and `struct X` differently, so `class ResTIMG;` in one header and
`struct ResTIMG { ... }` elsewhere produce unresolved externals at link time. MWCC doesn't care,
so the matching build is unaffected. Only global-namespace and simple namespace-scope forward
declarations of the form `class X;` / `struct X;` are rewritten.

Usage: python tools/pc/fix_class_struct.py [--dry-run]
"""

import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DIRS = ["include", "src", "libs/JSystem/include", "libs/nw4r/include", "libs/RVL_SDK/include",
        "libs/RVLFaceLib/include"]
EXTS = {".h", ".hpp", ".cpp", ".c"}

DEF = re.compile(r"^\s*(?:typedef\s+)?(class|struct)\s+([A-Za-z_]\w*)\s*(?:final\s*)?(?::[^;{]*)?\{", re.M)
FWD = re.compile(r"^(\s*)(class|struct)(\s+)([A-Za-z_]\w*)(\s*;)", re.M)


def files():
    for d in DIRS:
        for p in (ROOT / d).rglob("*"):
            if p.suffix in EXTS and p.is_file():
                yield p


def main():
    dry = "--dry-run" in sys.argv
    keys = defaultdict(set)
    texts = {}
    for p in files():
        try:
            t = p.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            t = p.read_text(encoding="latin-1")
        texts[p] = t
        for m in DEF.finditer(t):
            keys[m.group(2)].add(m.group(1))
    changed = 0
    for p, t in texts.items():
        def repl(m):
            name = m.group(4)
            k = keys.get(name)
            if not k or len(k) != 1:
                return m.group(0)
            want = next(iter(k))
            if want == m.group(2):
                return m.group(0)
            return f"{m.group(1)}{want}{m.group(3)}{name}{m.group(5)}"
        new = FWD.sub(repl, t)
        if new != t:
            changed += 1
            if dry:
                print(p.relative_to(ROOT))
            else:
                p.write_text(new, encoding="utf-8", newline="\n")
    ambiguous = [n for n, k in keys.items() if len(k) > 1]
    print(f"{'would change' if dry else 'changed'} {changed} files; {len(ambiguous)} names defined with both keys")


if __name__ == "__main__":
    main()
