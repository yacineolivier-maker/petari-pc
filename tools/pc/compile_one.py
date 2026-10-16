#!/usr/bin/env python3
"""Compile single source files of the PC build without ninja (safe to run in parallel).

Usage: python tools/pc/compile_one.py src/JSystem/JKernel/JKRHeap.cpp [more files...]

Needs the PC build configured once (build/pc/compile_commands.json). Sets up the MSVC
environment itself via vcvars64.bat. Prints the compiler errors and exits non-zero on failure.
"""
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
VCVARS = r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"


def compile_one(entry, arg):
    tmp = Path(tempfile.gettempdir())
    obj = tmp / f"smg_compile_one_{os.getpid()}.obj"
    # Redirect the object output so parallel runs don't fight over the ninja build tree.
    parts = [p for p in entry["command"].split() if not p.startswith(("/Fo", "/Fd"))]
    cmd = " ".join(parts) + f" /Fo{obj}"
    script = tmp / f"smg_compile_one_{os.getpid()}.cmd"
    body = ["@echo off", f'call "{VCVARS}" >nul', f'cd /d "{entry["directory"]}"', cmd, ""]
    script.write_text("\r\n".join(body), encoding="mbcs")
    res = subprocess.run(["cmd", "/c", str(script)], capture_output=True, text=True,
                         encoding="mbcs", errors="replace")
    out = res.stdout.splitlines() + res.stderr.splitlines()
    errors = [l for l in out if " error " in l or "fatal error" in l]
    status = "OK" if res.returncode == 0 else f"FAILED ({len(errors)} errors)"
    print(f"== {arg}: {status}")
    for line in errors[:80]:
        print(line.replace(str(ROOT) + "\\", ""))
    if res.returncode != 0 and not errors:
        print("\n".join(out[-20:]))
    return res.returncode == 0


def main():
    db = json.loads((ROOT / "build/pc/compile_commands.json").read_text())
    by_file = {Path(e["file"]).resolve(): e for e in db}
    ok = True
    for arg in sys.argv[1:]:
        entry = by_file.get((ROOT / arg).resolve())
        if entry is None:
            print(f"== {arg}: not in compile_commands.json (add its library to WW_PC_LIBS)")
            ok = False
            continue
        ok = compile_one(entry, arg) and ok
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
