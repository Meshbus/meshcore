#!/usr/bin/env python3
# Copyright (c) 2026 FoBE Studio
# SPDX-License-Identifier: MIT

"""Report public MeshCore API inventory and test coverage markers."""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path


PUBLIC_HEADERS = (
    Path("include/meshcore/runtime.h"),
    Path("include/meshcore/platform.h"),
    Path("include/meshcore/types.h"),
)
TEST_ROOTS = (Path("tests"),)
RUNTIME_HEADER = Path("include/meshcore/runtime.h")
PLATFORM_HEADER = Path("include/meshcore/platform.h")
TYPES_HEADER = Path("include/meshcore/types.h")
FAKE_PLATFORM = Path("tests/support/fake_platform.c")

FUNCTION_RE = re.compile(
    r"^\s*(?:int|void|bool|uint(?:8|16|32|64)_t|size_t|float|unsigned long)"
    r"\s+(meshcore_[a-zA-Z0-9_]+)\s*\("
)
TYPE_RE = re.compile(r"^\s*typedef\s+(?:struct|enum)\s+([a-zA-Z0-9_]+)")
CONSTANT_RE = re.compile(r"^\s*#define\s+(MESHCORE_[A-Z0-9_]+)\b")
COVERAGE_RE = re.compile(r"MESHCORE_API_COVERAGE:\s*([A-Za-z0-9_,\s]+)")
TYPE_COVERAGE_RE = re.compile(r"MESHCORE_TYPE_COVERAGE:\s*([A-Za-z0-9_,\s]+)")


@dataclass
class Coverage:
  files: dict[str, set[str]] = field(default_factory=dict)

  def add(self, symbol: str, path: Path, repo_root: Path) -> None:
    rel = path.relative_to(repo_root).as_posix()
    self.files.setdefault(symbol, set()).add(rel)


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser()
  parser.add_argument("--repo-root", default=".", help="MeshCore repository root")
  parser.add_argument(
      "--require-runtime-coverage",
      action="store_true",
      help="Fail when a public runtime API lacks a coverage marker",
  )
  parser.add_argument(
      "--require-type-coverage",
      action="store_true",
      help="Fail when a public type lacks a coverage marker",
  )
  parser.add_argument(
      "--require-fake-platform-hooks",
      action="store_true",
      help="Fail when tests/support/fake_platform.c lacks a platform hook",
  )
  return parser.parse_args()


def read_lines(path: Path) -> list[str]:
  return path.read_text(encoding="utf-8").splitlines()


def public_functions(path: Path) -> list[str]:
  symbols: list[str] = []
  for line in read_lines(path):
    match = FUNCTION_RE.match(line)
    if match:
      symbols.append(match.group(1))
  return symbols


def public_types(path: Path) -> list[str]:
  symbols: list[str] = []
  for line in read_lines(path):
    match = TYPE_RE.match(line)
    if match:
      symbols.append(match.group(1))
  return symbols


def public_constants(path: Path) -> list[str]:
  symbols: list[str] = []
  for line in read_lines(path):
    match = CONSTANT_RE.match(line)
    if match:
      symbol = match.group(1)
      if symbol.endswith("_H_"):
        continue
      symbols.append(symbol)
  return symbols


def implemented_functions(path: Path) -> set[str]:
  symbols: set[str] = set()
  if not path.is_file():
    return symbols
  for line in read_lines(path):
    match = FUNCTION_RE.match(line)
    if match:
      symbols.add(match.group(1))
  return symbols


def iter_test_files(repo_root: Path) -> list[Path]:
  files: list[Path] = []
  for root in TEST_ROOTS:
    full_root = repo_root / root
    if not full_root.is_dir():
      continue
    for path in full_root.rglob("*"):
      if path.suffix in {".c", ".h", ".cc", ".cpp", ".txt", ".md"}:
        files.append(path)
  return sorted(files)


def parse_coverage(repo_root: Path, marker_re: re.Pattern[str]) -> Coverage:
  coverage = Coverage()
  for path in iter_test_files(repo_root):
    text = path.read_text(encoding="utf-8", errors="ignore")
    for match in marker_re.finditer(text):
      for raw_symbol in match.group(1).replace(",", " ").split():
        coverage.add(raw_symbol.strip(), path, repo_root)
  return coverage


def print_section(title: str, symbols: list[str]) -> None:
  print(f"{title}: {len(symbols)}")
  for symbol in symbols:
    print(f"  {symbol}")


def main() -> int:
  args = parse_args()
  repo_root = Path(args.repo_root).resolve()

  for header in PUBLIC_HEADERS:
    if not (repo_root / header).is_file():
      print(f"missing public header: {header}", file=sys.stderr)
      return 1

  runtime_functions = public_functions(repo_root / RUNTIME_HEADER)
  platform_functions = public_functions(repo_root / PLATFORM_HEADER)
  public_type_symbols = public_types(repo_root / TYPES_HEADER) + public_types(
      repo_root / PLATFORM_HEADER
  )
  public_constant_symbols = public_constants(repo_root / TYPES_HEADER)
  runtime_coverage = parse_coverage(repo_root, COVERAGE_RE)
  type_coverage = parse_coverage(repo_root, TYPE_COVERAGE_RE)
  fake_platform_hooks = implemented_functions(repo_root / FAKE_PLATFORM)

  print("MeshCore API surface report")
  print("===========================")
  print_section("runtime functions", runtime_functions)
  print_section("platform hook functions", platform_functions)
  print_section("public types", public_type_symbols)
  print_section("public constants", public_constant_symbols)
  print("runtime API coverage markers:")
  missing: list[str] = []
  for symbol in runtime_functions:
    files = sorted(runtime_coverage.files.get(symbol, set()))
    if files:
      print(f"  PASS {symbol}: {', '.join(files)}")
    else:
      print(f"  MISS {symbol}")
      missing.append(symbol)

  type_missing: list[str] = []
  print("public type coverage markers:")
  for symbol in public_type_symbols:
    files = sorted(type_coverage.files.get(symbol, set()))
    if files:
      print(f"  PASS {symbol}: {', '.join(files)}")
    else:
      print(f"  MISS {symbol}")
      type_missing.append(symbol)

  hook_missing: list[str] = []
  print("fake platform hook implementations:")
  for symbol in platform_functions:
    if symbol in fake_platform_hooks:
      print(f"  PASS {symbol}: {FAKE_PLATFORM.as_posix()}")
    else:
      print(f"  MISS {symbol}")
      hook_missing.append(symbol)

  if args.require_runtime_coverage and missing:
    print(
        "missing runtime API coverage marker(s): " + ", ".join(missing),
        file=sys.stderr,
    )
    return 1
  if args.require_type_coverage and type_missing:
    print(
        "missing public type coverage marker(s): " + ", ".join(type_missing),
        file=sys.stderr,
    )
    return 1
  if args.require_fake_platform_hooks and hook_missing:
    print(
        "missing fake platform hook implementation(s): "
        + ", ".join(hook_missing),
        file=sys.stderr,
    )
    return 1

  return 0


if __name__ == "__main__":
  raise SystemExit(main())
