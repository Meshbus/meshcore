#!/usr/bin/env python3
# Copyright (c) 2026 FoBE Studio
# SPDX-License-Identifier: MIT

"""Install meshcore and verify a downstream CMake consumer can run."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser()
  parser.add_argument("--source-root", default=".", help="MeshCore source root")
  parser.add_argument(
      "--work-dir",
      default="build.meshcore-package-smoke",
      help="Scratch directory for package smoke build outputs",
  )
  parser.add_argument("--cmake", default="cmake", help="CMake executable")
  return parser.parse_args()


def run(command: list[str], cwd: Path) -> None:
  print("+ " + " ".join(command), flush=True)
  subprocess.run(command, cwd=cwd, check=True)


def find_minimal_host(consumer_build: Path) -> Path | None:
  names = ("meshcore_minimal_host", "meshcore_minimal_host.exe")
  for name in names:
    direct = consumer_build / name
    if direct.is_file():
      return direct
  for path in consumer_build.rglob("*"):
    if path.name in names and path.is_file():
      return path
  return None


def main() -> int:
  args = parse_args()
  source_root = Path(args.source_root).resolve()
  work_dir = Path(args.work_dir).resolve()
  install_build = work_dir / "install-build"
  install_prefix = work_dir / "prefix"
  consumer_build = work_dir / "consumer-build"

  if not (source_root / "CMakeLists.txt").is_file():
    print(f"missing MeshCore source root: {source_root}", file=sys.stderr)
    return 1

  if work_dir.exists():
    shutil.rmtree(work_dir)
  work_dir.mkdir(parents=True)

  run(
      [
          args.cmake,
          "-S",
          str(source_root),
          "-B",
          str(install_build),
          "-DMESHCORE_INSTALL=ON",
          "-DMESHCORE_BUILD_TESTS=OFF",
          "-DMESHCORE_BUILD_EXAMPLES=OFF",
      ],
      source_root,
  )
  run([args.cmake, "--build", str(install_build), "--parallel"], source_root)
  run(
      [
          args.cmake,
          "--install",
          str(install_build),
          "--prefix",
          str(install_prefix),
      ],
      source_root,
  )
  run(
      [
          args.cmake,
          "-S",
          str(source_root / "examples/minimal_host"),
          "-B",
          str(consumer_build),
          f"-DCMAKE_PREFIX_PATH={install_prefix}",
      ],
      source_root,
  )
  run([args.cmake, "--build", str(consumer_build), "--parallel"], source_root)

  exe = find_minimal_host(consumer_build)
  if exe is None:
    print("minimal host executable was not produced", file=sys.stderr)
    return 1

  result = subprocess.run(
      [str(exe)],
      cwd=consumer_build,
      check=False,
      text=True,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
  )
  if result.returncode != 0:
    print(result.stdout, end="")
    print(result.stderr, end="", file=sys.stderr)
    return result.returncode
  if "meshcore minimal host initialized" not in result.stdout:
    print("unexpected minimal host output:", file=sys.stderr)
    print(result.stdout, file=sys.stderr)
    return 1

  print(result.stdout, end="")
  print(f"package smoke: PASS ({install_prefix})")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
