# MeshCore Versioning

`meshcore` uses separate version signals for the C package, upstream evidence,
wire compatibility, and host-facing ABI. Do not collapse these into a single
upstream firmware version number.

## Version Signals

| Signal | Owner | Purpose |
| --- | --- | --- |
| C package version | `project(meshcore VERSION ...)` in `CMakeLists.txt` | Names this C library release and drives installed CMake package version checks. |
| Upstream evidence baseline | `upstream.lock` and `UPSTREAM.md` | Identifies the exact upstream MeshCore revision used as protocol/runtime evidence. |
| Wire payload version | Packet header `PAYLOAD_VER_*` evidence | Identifies the LoRa packet payload format accepted on the wire. |
| Host ABI version | `MESHCORE_ABI_VERSION` in `include/meshcore/types.h` | Lets host adapters detect breaking public C API or data-shape changes. |

The C package version answers "which release of this C library is installed?".
The upstream evidence baseline answers "which upstream behavior was this
release checked against?". The wire payload version answers "which MeshCore
packet format is accepted over LoRa?". The host ABI version answers "can this
host adapter compile against this public C boundary?".

## Current Compatibility Tuple

For the current tree:

| Field | Value |
| --- | --- |
| C package version | `0.1.0` |
| Upstream evidence commit | `e8d3c53ba1ea863937081cd0caad759b832f3028` |
| Nearest upstream companion tag | `companion-v1.16.0-8-ge8d3c53b` |
| Wire payload version | `PAYLOAD_VER_1`, encoded as `0x00` |
| Host ABI version | `27` |

This tuple is more precise than naming the C library `v1.16.0`. Upstream
`companion-v1.16.0` is a firmware/application release line, not a standalone
protocol or C ABI version. The current upstream evidence commit is also newer
than that tag.

## Git Tags

When the library is ready for a public release, tag the C package version with
an annotated tag:

```sh
git tag -a v0.1.0 -m "meshcore 0.1.0"
```

Do not create a tag only because the upstream firmware version changed. Update
the upstream evidence lock and release notes instead.

A release note should include the full compatibility tuple:

```text
C library version: 0.1.0
Upstream evidence: e8d3c53ba1ea863937081cd0caad759b832f3028
Nearest upstream tag: companion-v1.16.0-8-ge8d3c53b
Wire compatibility: PAYLOAD_VER_1 / encoded 0x00
Public ABI: 27
```

## Bump Rules

### C Package Version

Use SemVer for this library:

- bump `MAJOR` for incompatible public C API, ABI, or package-contract changes;
- bump `MINOR` for compatible public features or newly supported upstream
  behavior;
- bump `PATCH` for compatible bug fixes, parity fixes, tests, docs, and
  packaging corrections.

Before the first stable `1.0.0`, `0.x` versions may still change public
contracts more freely, but breaking changes must still update
`MESHCORE_ABI_VERSION`.

### Upstream Evidence Baseline

Update `upstream.lock` and `UPSTREAM.md` when the upstream evidence revision
changes. Classify the upstream changes before changing C implementation:
protocol core, promoted support, runtime evidence, host adapter evidence,
excluded, or deferred.

Changing the upstream evidence commit does not by itself require a public C ABI
bump. It does require rerunning the sync and boundary checks.

### Wire Payload Version

The upstream packet header reserves two bits for payload version. Current
upstream behavior accepts only `PAYLOAD_VER_1` (`0x00`) and rejects higher
payload versions.

Only change supported wire payload versions when upstream protocol evidence
does so. A new accepted payload version is a protocol-compatibility change and
must be called out in release notes.

### Host ABI Version

Bump `MESHCORE_ABI_VERSION` when a host adapter may need source changes or
compile-time gating because of public C boundary changes, including:

- public struct layout changes;
- public enum, constant, or limit semantic changes;
- `meshcore_runtime_*` call contract changes;
- `meshcore_platform_*` hook signature or callback semantic changes;
- removed or renamed public APIs.

Do not bump `MESHCORE_ABI_VERSION` for internal implementation changes, tests,
documentation, packaging metadata, or an upstream evidence update that leaves
the public host boundary unchanged.

## Pre-Release Checks

Before creating a release tag, run:

```sh
cmake -S . -B build.meshcore-native \
  -DMESHCORE_BUILD_TESTS=ON \
  -DMESHCORE_BUILD_EXAMPLES=ON
cmake --build build.meshcore-native
ctest --test-dir build.meshcore-native --output-on-failure
python3 tools/meshcore_sync_report.py --repo-root .
python3 tools/upstream_lock_check.py --repo-root .
```

Also install to a temporary prefix and verify that a downstream consumer can
use `find_package(meshcore CONFIG REQUIRED)` for the intended C package
version.
