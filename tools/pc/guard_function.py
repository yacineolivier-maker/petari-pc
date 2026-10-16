#!/usr/bin/env python3
"""Wrap a function definition in `#if !TARGET_PC ... #endif` (or `#if TARGET_PC` with --pc-only).

Used for library functions that the game replaces at link time on the Wii (src/Game/System/
Overwrite.cpp, src/Game/AudioLib/OverwriteJAudio.cpp): on PC both definitions would clash.

Usage: python tools/pc/guard_function.py <file> "<signature start>" [--reason "text"]
The signature start is matched at the start of a line (after indentation), e.g.
"JASAudioThread::JASAudioThread(" or "void JPADrawLine(".
"""

import argparse
import sys
from pathlib import Path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("file")
    ap.add_argument("signature")
    ap.add_argument("--reason", default="the game's version in Overwrite*.cpp replaces it")
    args = ap.parse_args()
    path = Path(args.file)
    lines = path.read_text(encoding="utf-8").split("\n")
    start = None
    for i, l in enumerate(lines):
        if l.lstrip().startswith(args.signature):
            start = i
            break
    if start is None:
        sys.exit(f"signature not found: {args.signature}")
    if start > 0 and lines[start - 1].strip().startswith("#if !TARGET_PC"):
        print("already guarded")
        return
    depth = 0
    seen_brace = False
    end = None
    for j in range(start, len(lines)):
        for c in lines[j]:
            if c == "{":
                depth += 1
                seen_brace = True
            elif c == "}":
                depth -= 1
        if seen_brace and depth == 0:
            end = j
            break
    if end is None:
        sys.exit("end of function not found")
    lines.insert(end + 1, "#endif")
    lines.insert(start, f"#if !TARGET_PC  // On PC {args.reason}.")
    path.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    print(f"guarded lines {start + 1}-{end + 1}")


if __name__ == "__main__":
    main()
