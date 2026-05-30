#!/usr/bin/env python3
# Copyright (c) 2026 FoBE Studio
# SPDX-License-Identifier: Apache-2.0

"""Verify that the checked-out MeshCore reference matches upstream.lock."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


LOCK_PATH = Path("upstream.lock")
REQUIRED_KEYS = {
    "reference_path",
    "commit",
    "protocol_core_root",
    "runtime_examples_root",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".", help="MeshCore repository root")
    parser.add_argument(
        "--lock",
        default=str(LOCK_PATH),
        help="Path to upstream lock file, relative to repo root",
    )
    return parser.parse_args()


def load_lock(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for lineno, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if "=" not in stripped:
            raise ValueError(f"{path}:{lineno}: expected key=value")
        key, value = stripped.split("=", 1)
        values[key.strip()] = value.strip()
    return values


def git_output(repo: Path, *args: str) -> str:
    result = subprocess.run(
        ["git", "-C", str(repo), *args],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip()
        raise RuntimeError(f"git {' '.join(args)} failed in {repo}: {detail}")
    return result.stdout.strip()


def main() -> int:
    args = parse_args()
    repo_root = Path(args.repo_root).resolve()
    lock_path = (repo_root / args.lock).resolve()

    if not lock_path.is_file():
        print(f"missing upstream lock: {lock_path}", file=sys.stderr)
        return 1

    try:
        lock = load_lock(lock_path)
    except ValueError as exc:
        print(exc, file=sys.stderr)
        return 1

    missing_keys = sorted(REQUIRED_KEYS - set(lock))
    if missing_keys:
        print(
            "missing upstream lock key(s): " + ", ".join(missing_keys),
            file=sys.stderr,
        )
        return 1

    reference = repo_root / lock["reference_path"]
    if not reference.is_dir():
        print(f"missing upstream reference path: {reference}", file=sys.stderr)
        return 1

    try:
        actual_commit = git_output(reference, "rev-parse", "HEAD")
        dirty = git_output(reference, "status", "--porcelain")
    except RuntimeError as exc:
        print(exc, file=sys.stderr)
        return 1

    expected_commit = lock["commit"]
    if actual_commit != expected_commit:
        print(
            "upstream reference drift:\n"
            f"  expected: {expected_commit}\n"
            f"  actual:   {actual_commit}",
            file=sys.stderr,
        )
        return 1

    if dirty:
        print(
            "upstream reference has local changes:\n" + dirty,
            file=sys.stderr,
        )
        return 1

    missing_roots = [
        key for key in ("protocol_core_root", "runtime_examples_root")
        if not (repo_root / lock[key]).exists()
    ]
    if missing_roots:
        print(
            "missing upstream evidence root(s): " + ", ".join(missing_roots),
            file=sys.stderr,
        )
        return 1

    print(
        "upstream lock check: PASS "
        f"({lock['reference_path']} @ {actual_commit[:12]})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
