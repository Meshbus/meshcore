#!/usr/bin/env python3
# Copyright (c) 2026 FoBE Studio
# SPDX-License-Identifier: Apache-2.0

"""Report MeshCore upstream-sync and architecture drift categories."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


UPSTREAM_DOC = Path("UPSTREAM.md")
PUBLIC_INCLUDE_ROOT = Path("include")
EXAMPLES_ROOT = Path("examples")
GENERIC_ROOTS = (Path("include"), Path("src"))
BUILD_SCAN_ROOTS = (Path("."),)
SKIP_SCAN_DIRS = {
    ".git",
    ".reference",
    ".references",
    ".ref",
    "build",
    "build.review",
    "build.meshcore-native",
    "build.meshcore-install",
    "cmake-build-debug",
    "cmake-build-release",
}
TEXT_SCAN_SUFFIXES = {".cmake", ".py", ".txt", ".yaml", ".yml"}
FORBIDDEN_GENERIC_RE = re.compile(
    r'#include\s*[<"]zephyr/|#include\s*[<"]Arduino|meshbus|Arduino'
)
OLD_SOURCE_ROOT_RE = re.compile(
    r"(?:^|[\s\"'`])(?:lib/meshcore/)?(?:protocol|runtime)/"
    r"|\$\{MESHCORE_LIB_DIR\}/(?:protocol|runtime)/"
    r"|(?:^|[\s\"'`])(?:lib/meshcore/)?src/port/compat(?:/|$)"
    r"|\$\{MESHCORE_LIB_DIR\}/src/port/compat(?:/|$)"
)
PUBLIC_SYSTEM_INCLUDES = {"stdbool.h", "stddef.h", "stdint.h"}
PUBLIC_PROJECT_INCLUDES = {
    "meshcore/types.h",
    "meshcore/runtime.h",
    "meshcore/platform.h",
}
EVIDENCE_SECTIONS = {
    "Protocol Core": "protocol core",
    "Promoted Support Helpers": "promoted support",
    "Runtime Evidence": "runtime evidence",
}
LEGACY_HOST_FUNCTIONS = (
    "meshcore_hal_radio_airtime",
    "meshcore_hal_radio_packet_score",
    "meshcore_hal_radio_begin",
    "meshcore_hal_radio_in_rx_mode_get",
    "meshcore_hal_radio_receiving_get",
    "meshcore_hal_radio_channel_active_get",
    "meshcore_hal_radio_frequency_get",
    "meshcore_hal_radio_noise_floor_calibrate",
    "meshcore_hal_radio_agc_reset",
    "meshcore_hal_radio_packet_send",
    "meshcore_hal_node_telemetry_get",
    "meshcore_hal_rng_random",
    "meshcore_hal_millis_get",
    "meshcore_hal_rtc_get_current_time",
    "meshcore_hal_sha256",
    "meshcore_hal_sha256_two_fragments",
    "meshcore_hal_aes128_encrypt_block",
    "meshcore_hal_aes128_decrypt_block",
    "meshcore_hal_hmac_sha256",
    "meshcore_node_identity_get",
    "meshcore_node_config_last_modify_get",
    "meshcore_node_runtime_policy_get",
    "meshcore_node_advert_profile_get",
    "meshcore_peer_identity_get_by_prefix",
    "meshcore_peer_identity_match_by_hash",
    "meshcore_peer_path_get_by_key",
    "meshcore_peer_path_update",
    "meshcore_peer_seen_update",
    "meshcore_channel_secret_get_by_hash",
    "meshcore_channel_secret_match_exists",
    "meshcore_channel_secret_hash",
    "meshcore_channel_payload_encrypt",
    "meshcore_channel_payload_decrypt",
    "meshcore_dispatcher_log_rx_raw",
    "meshcore_dispatcher_log_rx",
    "meshcore_dispatcher_log_tx",
    "meshcore_dispatcher_log_tx_fail",
    "meshcore_runtime_request_error",
    "meshcore_dispatcher_get_airtime_budget_factor",
    "meshcore_dispatcher_calc_rx_delay",
    "meshcore_dispatcher_get_cad_fail_max_duration",
    "meshcore_dispatcher_get_interference_threshold",
    "meshcore_dispatcher_get_agc_reset_interval",
    "meshcore_dispatcher_get_duty_cycle_window_ms",
    "meshcore_peer_payload_encrypt",
    "meshcore_peer_payload_decrypt",
    "meshcore_mesh_get_cad_fail_retry_delay",
    "meshcore_mesh_filter_recv_flood_packet",
    "meshcore_mesh_allow_packet_forward",
    "meshcore_mesh_get_retransmit_delay",
    "meshcore_mesh_get_direct_retransmit_delay",
    "meshcore_mesh_get_extra_ack_transmit_count",
    "meshcore_mesh_next_peer_shared_secret_by_hash",
    "meshcore_mesh_search_channels_by_hash",
    "meshcore_mesh_on_peer_data_recv",
    "meshcore_mesh_on_trace_recv",
    "meshcore_mesh_on_peer_path_recv",
    "meshcore_mesh_on_advert_recv",
    "meshcore_mesh_on_anon_data_recv",
    "meshcore_mesh_on_path_recv",
    "meshcore_mesh_on_control_data_recv",
    "meshcore_mesh_on_raw_data_recv",
    "meshcore_mesh_on_group_data_recv",
    "meshcore_mesh_on_ack_recv",
    "meshcore_message_handler",
    "meshcore_message_ack_handler",
    "meshcore_advert_handler",
    "meshcore_peer_path_publish",
    "meshcore_peer_path_handler",
    "meshcore_trace_path_handler",
    "meshcore_telemetry_handler",
    "meshcore_binary_response_handler",
    "meshcore_channel_data_handler",
    "meshcore_raw_data_handler",
    "meshcore_control_data_handler",
)
LEGACY_HOST_FUNCTION_RE = re.compile(
    r"\b(?:" + "|".join(re.escape(name) for name in LEGACY_HOST_FUNCTIONS) + r")\s*\("
)
PUBLIC_PRIVATE_CORE_TYPES = (
    "struct meshcore_packet",
    "struct meshcore_identity",
    "struct meshcore_group_channel",
)


@dataclass
class ReportItem:
    status: str
    category: str
    detail: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".", help="MeshCore repository root")
    return parser.parse_args()


def run_check(repo_root: Path, command: list[str]) -> tuple[int, str]:
    result = subprocess.run(
        command,
        cwd=repo_root,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    output = "\n".join(
        part for part in (result.stdout.strip(), result.stderr.strip()) if part
    )
    return result.returncode, output


def rel(path: Path, repo_root: Path) -> str:
    try:
        return str(path.relative_to(repo_root))
    except ValueError:
        return str(path)


def add(items: list[ReportItem], status: str, category: str, detail: str) -> None:
    items.append(ReportItem(status=status, category=category, detail=detail))


def parse_evidence_sections(repo_root: Path) -> dict[str, list[Path]]:
    upstream_path = repo_root / UPSTREAM_DOC
    sections: dict[str, list[Path]] = {
        label: [] for label in EVIDENCE_SECTIONS.values()
    }
    current: str | None = None

    for line in upstream_path.read_text(encoding="utf-8").splitlines():
        if line.startswith("### "):
            heading = line[4:].strip()
            current = EVIDENCE_SECTIONS.get(heading)
            continue
        if current is None:
            continue
        stripped = line.strip()
        if not stripped.startswith("- `"):
            continue
        parts = stripped.split("`")
        if len(parts) >= 2 and parts[1].startswith(".reference/meshcore/"):
            sections[current].append(Path(parts[1]))

    return sections


def check_upstream_lock(repo_root: Path, items: list[ReportItem]) -> None:
    rc, output = run_check(
        repo_root,
        [
            sys.executable,
            "tools/upstream_lock_check.py",
            "--repo-root",
            str(repo_root),
        ],
    )
    status = "PASS" if rc == 0 else "FAIL"
    if rc != 0 and "missing upstream reference path" in output:
        status = "WARN"
    add(
        items,
        status,
        "upstream lock",
        output or "upstream lock check produced no output",
    )


def check_evidence_files(repo_root: Path, items: list[ReportItem]) -> None:
    sections = parse_evidence_sections(repo_root)
    missing: list[str] = []

    if not (repo_root / ".reference/meshcore").is_dir():
        add(
            items,
            "WARN",
            "evidence files",
            "upstream reference checkout is not present; skip evidence file scan",
        )
        return

    for category, paths in sections.items():
        if not paths:
            missing.append(f"{category}: no evidence files listed")
            continue
        absent = [str(path) for path in paths if not (repo_root / path).is_file()]
        if absent:
            missing.extend(f"{category}: {path}" for path in absent)
        else:
            add(items, "PASS", category, f"{len(paths)} evidence file(s)")

    if missing:
        add(items, "FAIL", "evidence files", "\n".join(missing))


def check_subprocess_tools(repo_root: Path, items: list[ReportItem]) -> None:
    checks = [
        (
            "source manifest",
            [
                sys.executable,
                "tools/check_source_manifest.py",
                "--repo-root",
                str(repo_root),
            ],
        ),
    ]

    for category, command in checks:
        rc, output = run_check(repo_root, command)
        add(items, "PASS" if rc == 0 else "FAIL", category, output)


def iter_scan_files(root: Path) -> list[Path]:
    files: list[Path] = []
    if root.is_file():
        if root.name == "CMakeLists.txt" or root.suffix in TEXT_SCAN_SUFFIXES:
            return [root]
        return []
    if not root.exists():
        return files
    for path in root.rglob("*"):
        if any(
            part in SKIP_SCAN_DIRS or part.startswith("build.")
            for part in path.parts
        ):
            continue
        if not path.is_file():
            continue
        if path.name == "CMakeLists.txt" or path.suffix in TEXT_SCAN_SUFFIXES:
            files.append(path)
    return files


def check_public_header_docs(repo_root: Path, items: list[ReportItem]) -> None:
    violations: list[str] = []

    for path in (repo_root / PUBLIC_INCLUDE_ROOT / "meshcore").glob("*.h"):
        text = path.read_text(encoding="utf-8")
        rel_path = rel(path, repo_root)
        if "SPDX-License-Identifier:" not in text:
            violations.append(f"{rel_path}: missing SPDX license identifier")
        if "@file" not in text:
            violations.append(f"{rel_path}: missing @file documentation")
        if "@defgroup" not in text:
            violations.append(f"{rel_path}: missing Doxygen group")
        if "FOBE_LIB_" in text:
            violations.append(f"{rel_path}: FoBE path-style include guard")

    if violations:
        add(items, "FAIL", "public header docs", "\n".join(violations))
    else:
        add(
            items,
            "PASS",
            "public header docs",
            "public headers have SPDX, @file, Doxygen groups, and generic guards",
        )


def check_include_boundary(repo_root: Path, items: list[ReportItem]) -> None:
    matches: list[str] = []
    for root in GENERIC_ROOTS:
        for path in (repo_root / root).rglob("*"):
            if path.suffix not in {".c", ".h"}:
                continue
            text = path.read_text(encoding="utf-8")
            for lineno, line in enumerate(text.splitlines(), 1):
                if FORBIDDEN_GENERIC_RE.search(line):
                    matches.append(f"{rel(path, repo_root)}:{lineno}: {line.strip()}")

    if matches:
        add(items, "FAIL", "generic include boundary", "\n".join(matches))
    else:
        add(
            items,
            "PASS",
            "generic include boundary",
            "no Zephyr, Meshbus, or Arduino leakage in include/src C files",
        )


def check_public_header_ownership(repo_root: Path, items: list[ReportItem]) -> None:
    violations: list[str] = []
    include_re = re.compile(r'^\s*#include\s+([<"])([^>"]+)[>"]')

    for path in (repo_root / PUBLIC_INCLUDE_ROOT).rglob("*.h"):
        for lineno, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            match = include_re.match(line)
            if not match:
                continue
            delimiter, included = match.groups()
            allowed = (
                included in PUBLIC_SYSTEM_INCLUDES
                if delimiter == "<"
                else included in PUBLIC_PROJECT_INCLUDES
            )
            if not allowed:
                violations.append(
                    f"{rel(path, repo_root)}:{lineno}: disallowed include {included}"
                )

    if violations:
        add(items, "FAIL", "public header ownership", "\n".join(violations))
    else:
        add(
            items,
            "PASS",
            "public header ownership",
            "public headers include only standard C and public meshcore headers",
        )


def check_examples_boundary(repo_root: Path, items: list[ReportItem]) -> None:
    violations: list[str] = []
    include_re = re.compile(r'^\s*#include\s+([<"])([^>"]+)[>"]')
    examples_root = repo_root / EXAMPLES_ROOT

    if not examples_root.exists():
        add(items, "PASS", "examples boundary", "no examples directory present")
        return

    for path in examples_root.rglob("*"):
        if not path.is_file():
            continue
        if path.suffix in {".c", ".h"}:
            for lineno, line in enumerate(
                path.read_text(encoding="utf-8").splitlines(), 1
            ):
                match = include_re.match(line)
                if not match:
                    continue
                _, included = match.groups()
                if included in PUBLIC_PROJECT_INCLUDES:
                    continue
                if included.startswith("meshcore/"):
                    violations.append(
                        f"{rel(path, repo_root)}:{lineno}: unknown public include {included}"
                    )
                if (
                    included.startswith("meshcore_")
                    or included.startswith("../src/")
                    or "/src/" in included
                ):
                    violations.append(
                        f"{rel(path, repo_root)}:{lineno}: private include {included}"
                    )
        if path.name == "CMakeLists.txt":
            for lineno, line in enumerate(
                path.read_text(encoding="utf-8").splitlines(), 1
            ):
                if "MESHCORE_INTERNAL_INCLUDE_DIRS" in line or "/src/" in line:
                    violations.append(
                        f"{rel(path, repo_root)}:{lineno}: private include path {line.strip()}"
                    )

    if violations:
        add(items, "FAIL", "examples boundary", "\n".join(violations))
    else:
        add(
            items,
            "PASS",
            "examples boundary",
            "examples use public meshcore headers and target linkage only",
        )


def check_stale_paths(repo_root: Path, items: list[ReportItem]) -> None:
    matches: list[str] = []
    for root in BUILD_SCAN_ROOTS:
        for path in iter_scan_files(repo_root / root):
            text = path.read_text(encoding="utf-8")
            for lineno, line in enumerate(text.splitlines(), 1):
                if OLD_SOURCE_ROOT_RE.search(line):
                    matches.append(f"{rel(path, repo_root)}:{lineno}: {line.strip()}")

    if matches:
        add(items, "FAIL", "stale source roots", "\n".join(matches))
    else:
        add(
            items,
            "PASS",
            "stale source roots",
            "no build/tool references to retired source roots",
        )


def check_test_hook_boundary(repo_root: Path, items: list[ReportItem]) -> None:
    public_matches: list[str] = []
    unguarded_sources: list[str] = []

    for path in (repo_root / PUBLIC_INCLUDE_ROOT).rglob("*.h"):
        text = path.read_text(encoding="utf-8")
        if "meshcore_test_runtime_" in text:
            public_matches.append(rel(path, repo_root))

    for path in (repo_root / "src").rglob("*.c"):
        text = path.read_text(encoding="utf-8")
        if "meshcore_test_runtime_" in text and "MESHCORE_ENABLE_TEST_HOOKS" not in text:
            unguarded_sources.append(rel(path, repo_root))

    failures = []
    if public_matches:
        failures.append("public headers: " + ", ".join(sorted(public_matches)))
    if unguarded_sources:
        failures.append("unguarded sources: " + ", ".join(sorted(unguarded_sources)))

    if failures:
        add(items, "FAIL", "test hook boundary", "\n".join(failures))
    else:
        add(
            items,
            "PASS",
            "test hook boundary",
            "runtime white-box hooks are private and build-flag guarded",
        )


def check_platform_hook_boundary(repo_root: Path, items: list[ReportItem]) -> None:
    matches: list[str] = []
    private_type_matches: list[str] = []

    scan_paths = list((repo_root / PUBLIC_INCLUDE_ROOT).rglob("*.h"))
    scan_paths.extend(
        [
            repo_root / "src/platform/meshcore_platform_bridge.c",
        ]
    )

    for path in scan_paths:
        if not path.is_file():
            continue
        for lineno, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if LEGACY_HOST_FUNCTION_RE.search(line):
                matches.append(f"{rel(path, repo_root)}:{lineno}: {line.strip()}")
            if path.is_relative_to(repo_root / PUBLIC_INCLUDE_ROOT):
                for private_type in PUBLIC_PRIVATE_CORE_TYPES:
                    if private_type in line:
                        private_type_matches.append(
                            f"{rel(path, repo_root)}:{lineno}: {line.strip()}"
                        )

    failures = []
    if matches:
        failures.append("legacy host symbols:\n" + "\n".join(matches))
    if private_type_matches:
        failures.append(
            "private core types in public headers:\n"
            + "\n".join(private_type_matches)
        )

    if failures:
        add(items, "FAIL", "platform hook boundary", "\n\n".join(failures))
    else:
        add(
            items,
            "PASS",
            "platform hook boundary",
            "public headers and platform bridge avoid legacy host globals and private core types",
        )


def main() -> int:
    args = parse_args()
    repo_root = Path(args.repo_root).resolve()
    items: list[ReportItem] = []

    check_upstream_lock(repo_root, items)
    check_evidence_files(repo_root, items)
    check_subprocess_tools(repo_root, items)
    check_include_boundary(repo_root, items)
    check_public_header_ownership(repo_root, items)
    check_public_header_docs(repo_root, items)
    check_examples_boundary(repo_root, items)
    check_stale_paths(repo_root, items)
    check_test_hook_boundary(repo_root, items)
    check_platform_hook_boundary(repo_root, items)

    print("MeshCore sync report")
    print("====================")
    for item in items:
        print(f"[{item.status}] {item.category}: {item.detail}")

    failures = [item for item in items if item.status == "FAIL"]
    warnings = [item for item in items if item.status == "WARN"]
    print(
        "summary: "
        f"{len(items) - len(failures) - len(warnings)} pass, "
        f"{len(warnings)} warn, {len(failures)} fail"
    )
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
