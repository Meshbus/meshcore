#!/usr/bin/env python3
# Copyright (c) 2026 FoBE Studio
# SPDX-License-Identifier: MIT

"""Report upstream evidence references covered by native parity tests."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


UPSTREAM_DOC = Path("UPSTREAM.md")
TEST_ROOT = Path("tests")
EVIDENCE_RE = re.compile(r"\.reference/meshcore/[A-Za-z0-9_./+-]+")
EVIDENCE_SUFFIXES = {".h", ".hpp", ".c", ".cc", ".cpp", ".ino"}
PARITY_HINTS = ("parity", "oracle", "contract")


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser()
  parser.add_argument("--repo-root", default=".", help="MeshCore repository root")
  parser.add_argument(
      "--require-full-coverage",
      action="store_true",
      help="Fail when any upstream evidence file lacks a test reference",
  )
  return parser.parse_args()


def evidence_from_upstream(repo_root: Path) -> list[str]:
  text = (repo_root / UPSTREAM_DOC).read_text(encoding="utf-8")
  return sorted(
      {
          evidence
          for evidence in EVIDENCE_RE.findall(text)
          if Path(evidence).suffix in EVIDENCE_SUFFIXES
      }
  )


def iter_test_files(repo_root: Path) -> list[Path]:
  root = repo_root / TEST_ROOT
  if not root.is_dir():
    return []
  files: list[Path] = []
  for path in root.rglob("*"):
    if path.suffix in {".c", ".h", ".cc", ".cpp", ".md", ".txt"}:
      files.append(path)
  return sorted(files)


def evidence_from_tests(repo_root: Path) -> dict[str, set[str]]:
  coverage: dict[str, set[str]] = {}
  for path in iter_test_files(repo_root):
    text = path.read_text(encoding="utf-8", errors="ignore")
    lowered = path.name.lower() + "\n" + text.lower()
    if not any(hint in lowered for hint in PARITY_HINTS):
      continue
    rel = path.relative_to(repo_root).as_posix()
    for evidence in EVIDENCE_RE.findall(text):
      if Path(evidence).suffix not in EVIDENCE_SUFFIXES:
        continue
      coverage.setdefault(evidence, set()).add(rel)
  return coverage


def main() -> int:
  args = parse_args()
  repo_root = Path(args.repo_root).resolve()
  expected = evidence_from_upstream(repo_root)
  covered = evidence_from_tests(repo_root)

  print("MeshCore parity coverage report")
  print("===============================")
  print(f"upstream evidence files: {len(expected)}")
  print(f"evidence files referenced by tests: {len(covered)}")
  print("covered evidence:")
  for evidence in expected:
    files = sorted(covered.get(evidence, set()))
    if files:
      print(f"  PASS {evidence}: {', '.join(files)}")
  print("uncovered evidence:")
  uncovered = [evidence for evidence in expected if evidence not in covered]
  if not uncovered:
    print("  none")
  else:
    for evidence in uncovered:
      print(f"  MISS {evidence}")
    if args.require_full_coverage:
      return 1

  return 0


if __name__ == "__main__":
  raise SystemExit(main())
