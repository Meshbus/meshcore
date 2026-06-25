# MeshCore Test Plan

Status: implemented locally; CI policy draft for review

This plan is for automated CI of the standalone MeshCore C library. Its two
primary purposes are:

1. prove that protocol and observable runtime behavior stay compatible with the
   locked upstream Arduino MeshCore implementation;
2. test the public C API exhaustively enough that API misuse, boundary cases,
   host-hook failures, state-machine mistakes, and package/ABI regressions are
   caught before release.

The plan intentionally does not depend on the current FoBE Zephyr SDK firmware
implementation. FoBE board code, Zephyr services, Meshbus product services,
transport adapters, storage backends, UI, provisioning, radio hardware, west,
and Twister are outside this repository's required correctness gate.

## Review Findings

The previous plan had useful CI infrastructure coverage: native CTest, warning
builds, sanitizers, package smoke, upstream lock checks, coverage, and fuzzing.
However, it treated CI mechanics as the main structure. For this repository the
test plan must be organized around the two correctness claims above:

- Upstream parity is not satisfied by compiling the C library. It needs
  explicit upstream evidence mapping, byte-level fixtures, runtime oracle
  scenarios, and drift checks.
- API safety is not satisfied by a few smoke tests. Every public runtime entry
  point needs success, invalid input, boundary length, lifecycle, hook-failure,
  and observable side-effect coverage.
- Cross-platform CI, sanitizers, package smoke, and fuzzing are still important,
  but they are execution lanes for the parity/API suites, not the top-level
  test objective.

## Source Of Truth

### Layer 1: Protocol Core And Support

Upstream evidence files:

- `.reference/meshcore/src/Packet.h`
- `.reference/meshcore/src/Packet.cpp`
- `.reference/meshcore/src/Utils.h`
- `.reference/meshcore/src/Utils.cpp`
- `.reference/meshcore/src/Identity.h`
- `.reference/meshcore/src/Identity.cpp`
- `.reference/meshcore/src/Dispatcher.h`
- `.reference/meshcore/src/Dispatcher.cpp`
- `.reference/meshcore/src/Mesh.h`
- `.reference/meshcore/src/Mesh.cpp`
- `.reference/meshcore/src/MeshCore.h`
- `.reference/meshcore/src/helpers/StaticPoolPacketManager.h`
- `.reference/meshcore/src/helpers/StaticPoolPacketManager.cpp`
- `.reference/meshcore/src/helpers/SimpleMeshTables.h`
- `.reference/meshcore/src/helpers/AdvertDataHelpers.h`
- `.reference/meshcore/src/helpers/AdvertDataHelpers.cpp`
- `.reference/meshcore/src/helpers/TxtDataHelpers.h`
- `.reference/meshcore/src/helpers/TxtDataHelpers.cpp`

Target C surfaces:

- public constants and wire-visible limits in `include/meshcore/types.h`;
- packet, identity, utils, dispatcher, mesh, table, packet-manager, advert,
  text, telemetry, and crypto behavior under `src/core` and `src/support`;
- canonical source groups in `cmake/meshcore_sources.cmake`.

Expected validation style:

- byte-for-byte packet/golden-vector comparison where output is deterministic;
- deterministic boundary tests for all upstream packet/layout limits;
- C implementation tests tied to the evidence file that owns the behavior.

### Layer 2: Runtime

Upstream evidence files:

- `.reference/meshcore/src/helpers/BaseChatMesh.h`
- `.reference/meshcore/src/helpers/BaseChatMesh.cpp`
- `.reference/meshcore/src/helpers/TxtDataHelpers.h`
- `.reference/meshcore/src/helpers/TxtDataHelpers.cpp`
- `.reference/meshcore/src/helpers/AdvertDataHelpers.h`
- `.reference/meshcore/src/helpers/AdvertDataHelpers.cpp`
- `.reference/meshcore/src/helpers/ContactInfo.h`
- `.reference/meshcore/src/helpers/ChannelDetails.h`
- `.reference/meshcore/examples/companion_radio/main.cpp`
- `.reference/meshcore/examples/companion_radio/MyMesh.h`
- `.reference/meshcore/examples/companion_radio/MyMesh.cpp`
- `.reference/meshcore/examples/companion_radio/NodePrefs.h`
- `.reference/meshcore/examples/companion_radio/DataStore.h`
- `.reference/meshcore/examples/companion_radio/DataStore.cpp`

Target C surfaces:

- host-callable functions in `include/meshcore/runtime.h`;
- runtime implementation in `src/runtime`;
- host/platform publication and lookup hooks in `include/meshcore/platform.h`;
- fake host/oracle support under `tests/support`.

Expected validation style:

- tests enter through public `meshcore_*` runtime APIs;
- radio RX/TX, timer expiry, TX completion, and host storage are injected by a
  deterministic fake host;
- tests compare observable behavior: serialized radio frames, event callbacks,
  timer deadlines, pending tags, route choices, and host hook calls.

### Layer 3: Public Platform Boundary

Evidence files:

- `include/meshcore/types.h`
- `include/meshcore/runtime.h`
- `include/meshcore/platform.h`
- `src/platform/meshcore_platform_bridge.c`

Target C surfaces:

- every public runtime function;
- every host-implemented platform hook;
- every public struct, enum, constant, payload limit, and event data shape;
- installed CMake package target `meshcore::meshcore`.

Expected validation style:

- compile-only and runtime tests using public headers only;
- ABI snapshot and size/limit checks;
- link tests proving all required platform hooks are concrete symbols;
- install/consumer smoke through `find_package(meshcore CONFIG REQUIRED)`.

## Test Architecture

Recommended test layout:

```text
tests/
  api/                 public API contract tests
  parity/              upstream parity and golden-vector tests
  native/              CTest entry points, may be split by layer
  support/
    fake_platform.*    configurable fake host/platform harness
    oracle_host.*      runtime oracle helpers
    fixtures/          checked-in generated fixtures
  fuzz/                parser/runtime fuzz harnesses
```

Native tests can stay under `tests/native` initially, but the CTest targets
should be labelled by purpose:

- `api`
- `parity`
- `runtime`
- `boundary`
- `package`
- `fuzz`

Use `cmake/meshcore_sources.cmake` for all source lists. Full runtime tests use
`MESHCORE_RUNTIME_LIBRARY_SOURCES`. Focused Layer 1 tests can use smaller
manifest groups such as `MESHCORE_PROTOCOL_PACKET_SOURCES` or
`MESHCORE_SUPPORT_ADVERT_DATA_SOURCES`.

## Upstream Parity Strategy

### Parity Level 0: Evidence Lock And Mapping

Purpose: make sure tests are comparing against the intended upstream revision
and that every compatibility-relevant upstream file is classified.

Required checks:

```sh
python3 tools/upstream_lock_check.py --repo-root .
python3 tools/meshcore_sync_report.py --repo-root .
```

Rules:

- `meshcore_sync_report.py` is required on every PR.
- `upstream_lock_check.py` is required for releases, upstream-sync work, and
  manual/scheduled strict parity jobs.
- A public PR may run without `.reference/meshcore`; this is allowed only for
  repository-local boundary checks. Release parity requires the exact locked
  reference checkout.

Acceptance:

- all evidence files listed above are present in the locked reference checkout;
- `upstream.lock`, `UPSTREAM.md`, and `ARCHITECTURE.md` agree;
- every `src/*.c` file is covered by the source manifest;
- no generic `include/` or `src/` file leaks Zephyr, Meshbus, Arduino, board,
  transport, or UI dependencies.

### Parity Level 1: Golden Vectors

Purpose: compare deterministic byte-level behavior against upstream.

Fixture source:

- generate fixtures from the locked `.reference/meshcore` tree when feasible;
- otherwise hand-author fixtures from the upstream evidence file and cite the
  evidence file in the test name/comment.

Fixture policy:

- fixtures live under `tests/support/fixtures`;
- fixture filenames include the upstream evidence area, for example
  `packet_read_write_v1.json` or `advert_data_vectors.json`;
- fixture regeneration is manual or scheduled, never silent in ordinary PR CI;
- a changed fixture requires explaining whether upstream changed or the C port
  was previously wrong.

Required golden-vector areas:

| Area | Expected parity |
| --- | --- |
| Packet header fields | route bits, payload type bits, payload version bits. |
| Packet path encoding | hash size/count encoding, path byte length, invalid reserved hash size rejection. |
| Packet read/write | serialized bytes round-trip exactly match upstream `Packet::writeTo` / `readFrom`. |
| Packet hash | payload-type and payload hash behavior, including TRACE path-len caveat. |
| Payload type constants | `REQ`, `RESPONSE`, `TXT_MSG`, `ACK`, `ADVERT`, `GRP_TXT`, `GRP_DATA`, `ANON_REQ`, `PATH`, `TRACE`, `MULTIPART`, `CONTROL`, `RAW_CUSTOM`. |
| Identity | public/private key sizes, pub-key hex parsing, hash prefix matching, sign/verify, tamper rejection. |
| Utils | SHA fragments, encrypt/decrypt block behavior, MAC helpers, hex parser, text part parser. |
| Advert data | type/name/location/feature flags, max data length, invalid parser state. |
| TXT helpers | string copy/null padding, blank detection, hex parsing, group data type constants. |
| Tables | duplicate detection and ACK hash semantics. |
| Packet manager | fixed pool allocation, full pool behavior, delayed queue behavior. |
| Mesh packet builders | advert, peer datagram, anonymous datagram, group datagram, ACK, multi-ACK, path return, trace, raw data, control data. |

### Parity Level 2: Runtime Oracle

Purpose: prove that the host-driven C runtime publishes the same observable
behavior as upstream runtime evidence without copying Arduino scheduling.

Oracle input model:

- local node identity and role;
- local advert profile;
- runtime policy;
- peer records with public key, role, `is_neighbor`, `out_path_len`, `out_path`,
  last seen SNR, and shared secret;
- channel records with hash and secret;
- monotonic time and RTC;
- deterministic RNG stream;
- incoming raw radio frames;
- requested public runtime operation.

Oracle output model:

- return code;
- serialized outbound radio frames;
- timer arm/cancel calls and deadlines;
- platform events and event payloads;
- peer/channel lookup calls;
- peer seen/path update calls;
- request error callbacks;
- pending request tag and correlation behavior when visible through events.

Required runtime oracle scenarios:

| Scenario | Required parity behavior |
| --- | --- |
| Init/deinit | local identity failure propagates; success initializes radio/runtime; timer arms; deinit resets state. |
| Timer pump | expired deadlines process queued work and schedule the next deadline. |
| RX invalid frame | invalid or truncated radio frames fail without corrupting events or pending state. |
| TX done success/failure | dispatcher queue and pending response state advance according to upstream semantics. |
| Local advert | flood and zero-hop advert requests serialize expected advert packet/app data. |
| Peer advert replay | raw advert length and parser behavior match upstream; event payload preserves identity/app data. |
| Peer message direct | known neighbor zero-hop path sends direct, not flood. |
| Peer message unknown | non-neighbor with empty path or `MESHCORE_OUT_PATH_UNKNOWN` falls back to flood. |
| Peer message forced flood | `flood=true` overrides known direct path. |
| Peer ACK | expected ACK correlation publishes message ACK and handles duplicate ACKs. |
| Channel message | 16-byte and 32-byte secrets accepted; invalid secret lengths rejected; group text event target prefix matches secret prefix. |
| Channel data | data type, path, payload length, and event publication match group-data evidence. |
| Path discovery | generated/caller tags, path return, and peer path event behavior match companion semantics. |
| Trace path | path requirement, tag/auth fields, SNR arrays, and terminal event state match upstream behavior. |
| Telemetry request | permission-mask inversion and telemetry response bounds match upstream. |
| Binary request/response | tag correlation, flood return path bounds, direct response bounds, and binary response event payloads match upstream. |
| Node discover | role filters, prefix/full-key responses, timestamp filtering, and event payload shape match evidence. |
| Raw data | path and payload length boundaries match public contract and raw packet behavior. |
| Control data | zero-hop control requires bit 7 set in byte 0 and rejects invalid payloads. |

Important contact/path invariant:

- `out_path_len == 0` is direct zero-hop only when the host peer record is a
  neighbor.
- `out_path_len == 0` with `is_neighbor == false` means the path is unknown and
  peer sends must fall back to flood.
- Companion unknown/flood sentinel is `MESHCORE_OUT_PATH_UNKNOWN` (`0xff`).

### Parity Level 3: Differential Upstream Harness

Purpose: catch interpretation mistakes where hand-authored fixtures are
insufficient.

Initial scope:

- Layer 1 packet, advert, TXT, identity, and mesh packet builder behavior.
  Packet, advert, TXT, and inline identity helper behavior are suitable for the
  compiled upstream differential oracle. Mesh packet builders are covered by C
  golden-vector parity tests unless a future harness can compile upstream
  `Mesh.cpp` without importing excluded board, radio, scheduler, storage, or UI
  dependencies.

Approach:

- add an optional upstream oracle harness. The current implementation lives in
  `tools/upstream_oracle.py`: it compiles selected upstream files from
  `.reference/meshcore` in a scratch directory with minimal host stubs, links
  the current C library plus `tests/support/fake_platform.c`, and compares
  packet, advert, TXT, and inline identity helper behavior in-process;
- generate JSON or binary fixtures from the upstream harness;
- run the C tests against those fixtures.

Policy:

- this job is release/manual/scheduled at first;
- it is allowed to be skipped in public PRs without `.reference/meshcore`;
- it must not import board, RadioLib, BLE, Wi-Fi, display, button, sensor,
  serial, CLI, or concrete filesystem code.

## Public API Contract Strategy

Public API tests are not optional regression tests. They are the main proof
that this C library is safe for hosts to consume.

### API Surface Inventory

Add a small tool, `tools/api_surface_report.py`, that parses public headers and
emits:

- runtime functions from `include/meshcore/runtime.h`;
- platform hook functions from `include/meshcore/platform.h`;
- public structs/enums/constants from `include/meshcore/types.h`;
- a coverage table mapping each public runtime function to at least one test
  file and CTest label.

PR acceptance after the tool exists:

- every public runtime function must be listed in the API test coverage table;
- every public struct/event type must have at least one size/field/limit or
  event serialization test;
- every public constant that maps to upstream protocol or ABI behavior must be
  checked in a boundary test.

### Runtime API Test Matrix

Every function in `include/meshcore/runtime.h` needs these categories unless
the function contract makes one category impossible:

- success path;
- call before `meshcore_init`;
- call after `meshcore_deinit`;
- null pointer rejection for every pointer parameter;
- zero length, one byte, maximum length, and maximum-plus-one rejection for
  every buffer parameter;
- invalid enum/flag/type values;
- host hook failure propagation;
- no outbound frame or event on rejected input;
- expected outbound frame/event/timer side effects on accepted input;
- repeated call behavior where state is involved;
- sanitizer-clean execution.

Required runtime API coverage:

| Public function | Required API tests |
| --- | --- |
| `meshcore_init` | success, identity hook failure, policy hook failure if applicable, timer-arm failure, repeated init, deinit after failed init. |
| `meshcore_deinit` | deinit before init, deinit after init, repeated deinit, no stale timer/event state. |
| `meshcore_timer_fired` | before init, valid deadline, old/new timestamp, timer hook failure, next deadline scheduling. |
| `meshcore_radio_rx_inject` | null data, zero len, max MTU, max-plus-one, invalid packet, valid packet event, SNR/RSSI propagation. |
| `meshcore_radio_tx_done` | before init, no active TX, success, failure, queue advance, pending timeout interaction. |
| `meshcore_node_advert_request` | flood true/false, before init, radio send failure, expected advert frame. |
| `meshcore_node_peer_advert_request` | null advert, zero len, max advert, invalid advert, valid advert event. |
| `meshcore_message_send_to_node` | null key, null payload, len 0/1/max/max+1, forced flood, known direct, unknown path flood, neighbor zero-hop direct, ACK attempt. |
| `meshcore_message_send_to_channel` | null secret, secret len 0/15/16/32/33, null payload, payload len boundaries, expected group text frame. |
| `meshcore_channel_data_send` | secret bounds, null path with zero len, non-null path, invalid path len, reserved/dev data type, payload bounds. |
| `meshcore_node_discover_path_request` | null key, null tag, zero tag generation, caller tag preservation, path known/unknown behavior. |
| `meshcore_node_trace_path_request` | null key, no path, known path, null tag, caller tag, expected trace frame. |
| `meshcore_node_telemetry_request` | null key, permission masks 0/all/base/location/environment, tag generation, payload encoding. |
| `meshcore_node_binary_request` | null key, null payload, payload len 0/max/max+1, generated tag. |
| `meshcore_node_binary_request_with_tag` | all binary request tests plus caller tag, duplicate tag behavior. |
| `meshcore_node_anon_data_send` | null key, null payload, payload len 0/max/max+1, expected anonymous datagram. |
| `meshcore_node_binary_response` | null request, null payload, direct response max, flood response reduced max, return path bounds. |
| `meshcore_node_discover_request` | filter none/all/role-specific, prefix/full key, since 0/nonzero, tag generation. |
| `meshcore_raw_data_send` | null path, path len 0/max/max+1, null payload, payload max/max+1. |
| `meshcore_control_data_send` | null payload, len 0/max/max+1, byte0 bit7 unset/set, expected zero-hop control frame. |

### Platform Hook Contract Tests

The platform header is implemented by hosts, but this repository must still
test the runtime's expectations of those hooks.

Required contract tests:

- fake platform defines every `meshcore_platform_*` symbol declared by
  `include/meshcore/platform.h`;
- a negative link test or generated symbol inventory detects newly added hooks
  that fake hosts do not implement;
- each hook that returns a value can be configured to return success/failure;
- each event hook records event count and a copy of the last event payload;
- each lookup hook can be configured for hit, miss, invalid data, and failure;
- timer and radio hooks record every call in order;
- crypto/RNG hooks support deterministic vectors and forced failure;
- platform callbacks do not call back into `meshcore_*` APIs.

Required hook groups:

- runtime scheduling hooks;
- time source hooks;
- radio hooks;
- RNG and crypto hooks;
- node identity/config/policy hooks;
- peer and channel lookup hooks;
- dispatcher policy/logging hooks;
- mesh behavior hooks;
- runtime event publication hooks;
- telemetry provider hook.

### Public Type And ABI Tests

Required tests:

- `MESHCORE_ABI_VERSION` is checked deliberately.
- all upstream payload type constants match upstream values.
- all public maximum lengths are tested at `max` and `max + 1`.
- `MESHCORE_OUT_PATH_UNKNOWN == 0xff`.
- `sizeof` and selected `offsetof` checks exist for public event structs.
- event structs preserve copied payload lengths and do not expose library-owned
  private pointers except documented borrowed packet views.
- headers compile as C and C++.
- installed headers do not include private `src/` headers.
- a minimal consumer builds using only `include/meshcore/*.h` and
  `meshcore::meshcore`.

## CI Gates

### Required PR Gate

Purpose: run the smallest set that proves parity/API regressions are unlikely.

Required jobs:

- `sync-boundary`: `python3 tools/meshcore_sync_report.py --repo-root .`
- `api-contract`: all public runtime API and public type tests.
- `parity-fixtures`: deterministic Layer 1 golden-vector tests.
- `runtime-oracle-smoke`: high-value Layer 2 scenarios: init, invalid RX,
  direct/flood/unknown peer send, channel message, binary response bounds.
- `strict-warnings`: Linux GCC or Clang with
  `-Wall -Wextra -Werror -Wpedantic`.
- `asan-ubsan`: Linux Clang sanitizer run.
- `package-smoke`: install to temporary prefix and build minimal consumer.

Baseline commands:

```sh
python3 tools/meshcore_sync_report.py --repo-root .
cmake -S . -B build.meshcore-native \
  -DMESHCORE_BUILD_TESTS=ON \
  -DMESHCORE_BUILD_EXAMPLES=ON
cmake --build build.meshcore-native --parallel
ctest --test-dir build.meshcore-native --output-on-failure
```

### Cross-Platform Gate

Purpose: catch compiler, path, and C ABI portability issues.

Recommended matrix:

| OS | Compiler family | Requirement |
| --- | --- | --- |
| Ubuntu 24.04 | GCC | required |
| Ubuntu 24.04 | Clang | required |
| macOS 15 | AppleClang | required |
| Windows 2025 | MSVC | advisory until first stable green run, then required |

Cross-platform jobs should run the same API/parity CTest labels. They are not a
substitute for the API/parity suites.

### Strict Upstream Parity Gate

Purpose: prove compatibility against the locked reference tree.

Trigger:

- release branch;
- release tag;
- upstream evidence update;
- manual `workflow_dispatch`;
- scheduled weekly job.

Required setup:

```sh
mkdir -p .reference
git clone https://github.com/meshcore-dev/MeshCore .reference/meshcore
git -C .reference/meshcore fetch origin e8d3c53ba1ea863937081cd0caad759b832f3028
git -C .reference/meshcore checkout --detach e8d3c53ba1ea863937081cd0caad759b832f3028
```

Required checks:

```sh
python3 tools/upstream_lock_check.py --repo-root .
python3 tools/meshcore_sync_report.py --repo-root .
ctest --test-dir build.meshcore-native -L parity --output-on-failure
```

When the optional upstream differential harness exists, run it in this gate.

### Fuzz And Stress Gate

Purpose: find API and parser bugs beyond deterministic fixtures.

Trigger:

- nightly;
- manual;
- release branch.

Targets:

- packet read parser;
- advert data parser;
- TXT helper parser;
- Cayenne LPP compatibility parser;
- `meshcore_radio_rx_inject` with random frames;
- public runtime API boundary fuzz for length and null combinations.

Policy:

- fuzzing does not need FoBE firmware;
- crash reproducers become deterministic API or parity regression tests;
- nightly fuzz failures block releases until triaged.

### Coverage Gate

Purpose: measure API and parity coverage, not just line coverage.

Metrics:

- public runtime functions covered / total public runtime functions;
- public event structs covered / total public event structs;
- upstream evidence areas with at least one parity test;
- branch/line coverage for parser and runtime request code.

The current report implementation is `tools/coverage_report.py`. It consumes a
GCC/Clang gcov-style coverage build after CTest and prints all `src/` files plus
focused parser/support and runtime request groups.

Policy:

- initially report only;
- after API inventory tooling exists, require 100% public runtime function
  inventory coverage;
- do not use a single global line coverage number as a release criterion.

## Required Checks By Change Type

| Change type | Required checks |
| --- | --- |
| Public runtime API change | API inventory update, API contract tests, type/ABI tests, package smoke, cross-platform native. |
| Public platform hook change | fake platform symbol inventory, hook failure tests, link smoke, package smoke. |
| Public type/constant change | ABI/version decision, size/offset tests, max/max+1 API tests, upstream parity mapping. |
| Layer 1 protocol/support change | golden vectors, relevant Layer 1 parity tests, sanitizer, strict upstream gate if evidence touched. |
| Layer 2 runtime change | runtime oracle tests, API contract tests for touched entry points, sanitizer, strict upstream gate if evidence touched. |
| Layer 3 boundary/package change | public-header tests, install/consumer smoke, cross-platform native. |
| Upstream evidence update | strict upstream lock, fixture regeneration or explicit deferral, changed-layer parity tests. |
| Documentation-only change | sync-boundary unless docs change architecture/evidence/release claims. |
| Release tag | PR gate, cross-platform gate, strict upstream parity gate, package smoke, coverage report, fuzz/stress review. |

## Pass/Fail Policy

Required PR failures:

- API inventory reports an untested public runtime function.
- Any API contract CTest fails.
- Any parity fixture test fails.
- Any runtime oracle smoke scenario fails.
- `meshcore_sync_report.py` reports a failure.
- CMake configure/build fails.
- warning-as-error build fails after enabled.
- sanitizer reports a finding after enabled.
- install/consumer smoke fails after enabled.

Allowed PR warnings:

- missing `.reference/meshcore` warning from sync report, only when
  repository-local checks pass;
- Windows advisory failure during bring-up;
- coverage percentage below future thresholds before coverage policy is active.

Required release failures:

- missing, dirty, or wrong `.reference/meshcore`;
- fixture mismatch without an approved upstream-change explanation;
- public ABI change without deliberate `MESHCORE_ABI_VERSION` decision;
- any public runtime function without API contract coverage;
- package cannot be consumed through `find_package(meshcore CONFIG REQUIRED)`.

## Implementation Order

1. Add CTest labels and split existing tests into `api`, `parity`, `runtime`,
   and `boundary` groups without changing public ABI.
2. Add `tools/api_surface_report.py` and make it report untested public runtime
   functions.
3. Expand `tests/support/fake_platform.*` into a configurable fake host:
   captured TX frames, event arrays, call order, peer/channel records, failure
   injection, deterministic time/RNG.
4. Add API contract tests for every function in `include/meshcore/runtime.h`.
5. Add Layer 1 golden-vector parity tests for packet, advert, text, identity,
   table, packet manager, and mesh packet-builder behavior.
6. Add runtime oracle tests for direct/flood/unknown path, ACK/pending,
   telemetry, binary request/response, node discover, raw data, and control
   data behavior.
7. Add package smoke helper for installed consumer testing.
8. Add CI workflow with required PR gate.
9. Add strict upstream parity workflow with locked `.reference/meshcore`.
10. Add fuzz/stress and coverage jobs.

## Acceptance Criteria

The plan is implemented when:

- all public runtime functions have API contract coverage in the inventory
  report;
- all public payload limits are tested at valid max and invalid max-plus-one;
- all public event structs are covered by at least one publication or ABI test;
- all Layer 1 protocol/support evidence areas have golden-vector or explicit
  parity tests;
- Layer 2 runtime oracle covers direct, flood, unknown path, neighbor zero-hop,
  ACK, pending request, timer, radio RX/TX, telemetry, binary response, node
  discover, raw data, and control data;
- strict upstream evidence checks pass against the locked `.reference/meshcore`
  revision;
- API/parity tests pass under native CTest without Zephyr, west, Twister, FoBE
  firmware, board hardware, or product services;
- install/consumer smoke proves `find_package(meshcore CONFIG REQUIRED)` and
  `meshcore::meshcore` work from a temporary prefix;
- sanitizer and strict warning jobs are clean on Linux.

## Current Local Validation

These checks were run from the repository root while implementing and reviewing
the CI test plan:

```sh
cmake -S . -B build.meshcore-native \
  -DMESHCORE_BUILD_TESTS=ON \
  -DMESHCORE_BUILD_EXAMPLES=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build.meshcore-native --parallel
ctest --test-dir build.meshcore-native --output-on-failure
python3 tools/api_surface_report.py --repo-root . \
  --require-runtime-coverage \
  --require-type-coverage \
  --require-fake-platform-hooks
python3 tools/parity_coverage_report.py --repo-root .
python3 tools/meshcore_sync_report.py --repo-root .
python3 tools/upstream_lock_check.py --repo-root .
```

Observed result:

- native CMake/CTest passed: 13 tests passed;
- API inventory passed: 20/20 runtime functions, 23/23 public types, and 69/69
  fake platform hooks covered;
- parity coverage report passed: 28/28 upstream evidence files referenced by
  parity/oracle/contract tests;
- sync report passed: 12 pass, 0 warn, 0 fail;
- upstream lock check passed for `.reference/meshcore` at
  `e8d3c53ba1ea`;
- strict warning build with AppleClang and
  `-Wall -Wextra -Werror -Wpedantic` passed: 13 tests passed;
- ASan/UBSan smoke build with AppleClang passed: 13 tests passed;
- coverage build passed: 13 tests passed and `tools/coverage_report.py`
  generated parser/support and runtime request focus reports;
- install/consumer smoke passed: `examples/minimal_host` consumed the installed
  package and printed `meshcore minimal host initialized`.
