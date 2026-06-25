#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
#
# Copyright (c) 2026 FoBE Studio

"""Report native line/branch coverage for MeshCore source files."""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


FILE_RE = re.compile(r"^File '(.+)'$")
LINES_RE = re.compile(r"^Lines executed:([0-9.]+)% of ([0-9]+)")
BRANCHES_RE = re.compile(r"^Branches executed:([0-9.]+)% of ([0-9]+)")

PARSER_SOURCES = {
    Path("src/core/meshcore_packet.c"),
    Path("src/support/meshcore_advert_data.c"),
    Path("src/support/meshcore_txt_data.c"),
    Path("src/support/telemetry/meshcore_cayenne_lpp_compat.c"),
}
RUNTIME_REQUEST_SOURCES = {
    Path("src/runtime/meshcore_runtime_request.c"),
    Path("src/runtime/meshcore_runtime_pending.c"),
    Path("src/runtime/meshcore_runtime_receive.c"),
    Path("src/runtime/meshcore_runtime_control.c"),
}


@dataclass
class CoverageRecord:
  source: Path
  lines_percent: float | None = None
  lines_total: int = 0
  branches_percent: float | None = None
  branches_total: int = 0


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--repo-root", default=".", help="repository root")
  parser.add_argument("--build-dir", required=True, help="coverage build directory")
  parser.add_argument("--gcov", default=None, help="gcov executable")
  parser.add_argument(
      "--require-data",
      action="store_true",
      help="fail when no gcov data is present",
  )
  return parser.parse_args()


def run_gcov(gcov: str, gcda: Path, output_dir: Path) -> str:
  result = subprocess.run(
      [gcov, "-b", "-c", "-o", str(gcda.parent), str(gcda)],
      cwd=output_dir,
      text=True,
      stdout=subprocess.PIPE,
      stderr=subprocess.STDOUT,
      check=False,
  )
  if result.returncode != 0:
    raise RuntimeError(result.stdout)
  return result.stdout


def parse_gcov_stdout(text: str, repo_root: Path) -> list[CoverageRecord]:
  records: list[CoverageRecord] = []
  current: CoverageRecord | None = None
  for raw_line in text.splitlines():
    line = raw_line.strip()
    file_match = FILE_RE.match(line)
    if file_match:
      source = Path(file_match.group(1))
      if not source.is_absolute():
        source = (repo_root / source).resolve()
      try:
        rel = source.resolve().relative_to(repo_root)
      except ValueError:
        current = None
        continue
      if rel.parts and rel.parts[0] == "src" and source.suffix == ".c":
        current = CoverageRecord(rel)
        records.append(current)
      else:
        current = None
      continue
    if current is None:
      continue
    lines_match = LINES_RE.match(line)
    if lines_match:
      current.lines_percent = float(lines_match.group(1))
      current.lines_total = int(lines_match.group(2))
      continue
    branches_match = BRANCHES_RE.match(line)
    if branches_match:
      current.branches_percent = float(branches_match.group(1))
      current.branches_total = int(branches_match.group(2))
  return records


def merge_records(records: list[CoverageRecord]) -> dict[Path, CoverageRecord]:
  # gcov can report the same source more than once when object files are rebuilt.
  # Keep the record with the largest line denominator as the most useful report.
  merged: dict[Path, CoverageRecord] = {}
  for record in records:
    previous = merged.get(record.source)
    if previous is None or record.lines_total >= previous.lines_total:
      merged[record.source] = record
  return dict(sorted(merged.items(), key=lambda item: item[0].as_posix()))


def print_records(title: str, records: list[CoverageRecord]) -> None:
  print(f"{title}: {len(records)} file(s)")
  if not records:
    print("  none")
    return
  for record in sorted(records, key=lambda item: item.source.as_posix()):
    lines = "n/a"
    branches = "n/a"
    if record.lines_percent is not None:
      lines = f"{record.lines_percent:.2f}% of {record.lines_total}"
    if record.branches_percent is not None:
      branches = f"{record.branches_percent:.2f}% of {record.branches_total}"
    print(f"  {record.source}: lines {lines}, branches {branches}")


def main() -> int:
  args = parse_args()
  repo_root = Path(args.repo_root).resolve()
  build_dir = Path(args.build_dir).resolve()
  gcov = args.gcov or shutil.which("gcov")
  if not gcov:
    print("gcov not found; coverage report skipped")
    return 1 if args.require_data else 0
  if not build_dir.is_dir():
    print(f"coverage build directory not found: {build_dir}", file=sys.stderr)
    return 1

  gcda_files = sorted(build_dir.rglob("*.gcda"))
  if not gcda_files:
    print("MeshCore coverage report")
    print("========================")
    print("no gcov data found; run a --coverage build and CTest first")
    return 1 if args.require_data else 0

  output_dir = build_dir / "coverage-gcov"
  output_dir.mkdir(parents=True, exist_ok=True)
  records: list[CoverageRecord] = []
  for gcda in gcda_files:
    try:
      records.extend(parse_gcov_stdout(run_gcov(gcov, gcda, output_dir), repo_root))
    except RuntimeError as exc:
      print(f"gcov failed for {gcda}:\n{exc}", file=sys.stderr)
      return 1

  merged = merge_records(records)
  all_records = list(merged.values())
  parser_records = [record for record in all_records if record.source in PARSER_SOURCES]
  runtime_records = [
      record for record in all_records if record.source in RUNTIME_REQUEST_SOURCES
  ]

  print("MeshCore coverage report")
  print("========================")
  print_records("all src coverage", all_records)
  print_records("parser/support focus", parser_records)
  print_records("runtime request focus", runtime_records)

  if args.require_data and not all_records:
    print("gcov data did not include MeshCore src files", file=sys.stderr)
    return 1
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
