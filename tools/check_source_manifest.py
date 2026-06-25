#!/usr/bin/env python3
# Copyright (c) 2026 FoBE Studio
# SPDX-License-Identifier: MIT

"""Check that every MeshCore C source is listed in the CMake manifest."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


MANIFEST = Path("cmake/meshcore_sources.cmake")
SOURCE_ROOTS = (Path("src"),)
PATH_RE = re.compile(r"\$\{MESHCORE_LIB_DIR\}/([^\s)]+\.c)")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".", help="MeshCore repository root")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = Path(args.repo_root).resolve()
    manifest_path = repo_root / MANIFEST

    if not manifest_path.is_file():
        print(f"missing source manifest: {MANIFEST}", file=sys.stderr)
        return 1

    text = manifest_path.read_text(encoding="utf-8")
    matches = [Path(match) for match in PATH_RE.findall(text)]
    listed = set(matches)

    actual = set()
    for root in SOURCE_ROOTS:
        actual.update(path.relative_to(repo_root) for path in (repo_root / root).rglob("*.c"))

    stale = sorted(path for path in listed if not (repo_root / path).is_file())
    missing = sorted(path for path in actual if path not in listed)
    duplicates = sorted(path for path in listed if matches.count(path) > 1)

    if stale:
        print("stale manifest source(s):", file=sys.stderr)
        for path in stale:
            print(f"  {path}", file=sys.stderr)

    if missing:
        print("missing manifest source(s):", file=sys.stderr)
        for path in missing:
            print(f"  {path}", file=sys.stderr)

    if duplicates:
        print("duplicate manifest source(s):", file=sys.stderr)
        for path in duplicates:
            print(f"  {path}", file=sys.stderr)

    if stale or missing or duplicates:
        return 1

    print(f"source manifest check: PASS ({len(actual)} source file(s))")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
