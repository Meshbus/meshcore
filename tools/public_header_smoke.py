#!/usr/bin/env python3
# Copyright (c) 2026 FoBE Studio
# SPDX-License-Identifier: MIT

"""Compile public MeshCore headers as C and C++ translation units."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


HEADER_SMOKE_SOURCE = """
#include <meshcore/types.h>
#include <meshcore/platform.h>
#include <meshcore/runtime.h>

int main(void) {
  return MESHCORE_ABI_VERSION == 0U;
}
"""


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser()
  parser.add_argument("--repo-root", default=".", help="MeshCore repository root")
  parser.add_argument(
      "--work-dir",
      default="build.meshcore-header-smoke",
      help="Scratch directory for object files",
  )
  parser.add_argument("--cc", default="", help="C compiler")
  parser.add_argument("--cxx", default="", help="C++ compiler")
  return parser.parse_args()


def pick_compiler(explicit: str, candidates: tuple[str, ...]) -> str | None:
  if explicit:
    return explicit
  for candidate in candidates:
    found = shutil.which(candidate)
    if found is not None:
      return found
  return None


def compile_source(compiler: str, source: Path, output: Path, include_dir: Path,
                   language: str) -> None:
  is_msvc = Path(compiler).name.lower() in {"cl", "cl.exe"}
  if is_msvc:
    mode = "/TC" if language == "c" else "/TP"
    command = [
        compiler,
        "/nologo",
        mode,
        f"/I{include_dir}",
        "/c",
        str(source),
        f"/Fo{output}",
    ]
  else:
    std = "-std=c99" if language == "c" else "-std=c++17"
    command = [
        compiler,
        std,
        "-Wall",
        "-Wextra",
        "-Werror",
        f"-I{include_dir}",
        "-c",
        str(source),
        "-o",
        str(output),
    ]

  print("+ " + " ".join(command), flush=True)
  subprocess.run(command, check=True)


def main() -> int:
  args = parse_args()
  repo_root = Path(args.repo_root).resolve()
  include_dir = repo_root / "include"
  work_dir = Path(args.work_dir).resolve()
  c_source = work_dir / "meshcore_public_header_smoke.c"
  cxx_source = work_dir / "meshcore_public_header_smoke.cpp"

  if not (include_dir / "meshcore" / "runtime.h").is_file():
    print(f"missing public include tree: {include_dir}", file=sys.stderr)
    return 1

  work_dir.mkdir(parents=True, exist_ok=True)
  c_source.write_text(HEADER_SMOKE_SOURCE, encoding="utf-8")
  cxx_source.write_text(HEADER_SMOKE_SOURCE, encoding="utf-8")

  cc = pick_compiler(args.cc, ("cc", "clang", "gcc", "cl"))
  cxx = pick_compiler(args.cxx, ("c++", "clang++", "g++", "cl"))
  if cc is None:
    print("no C compiler found for public header smoke", file=sys.stderr)
    return 1
  if cxx is None:
    print("no C++ compiler found for public header smoke", file=sys.stderr)
    return 1

  compile_source(cc, c_source, work_dir / "meshcore_public_header_smoke.o",
                 include_dir, "c")
  compile_source(cxx, cxx_source, work_dir / "meshcore_public_header_smoke_cpp.o",
                 include_dir, "cxx")
  print("public header smoke: PASS")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
