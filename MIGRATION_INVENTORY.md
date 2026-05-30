# MeshCore Stabilization Inventory

This inventory started as the Phase 0 baseline for the MeshCore library
architecture stabilization plan and is kept current as the platform-hook ABI
migration removes compatibility fallbacks.

## Public Header Ownership

| Header | Current contents | Target owner |
| --- | --- | --- |
| `include/meshcore/types.h` | ABI version, public limits, shared MeshCore constants, and common node/peer/channel/event data shapes. | Canonical type and limit owner. |
| `include/meshcore/runtime.h` | Singleton lifecycle, event injection, and typed request APIs. | Canonical callable runtime API owner. |
| `include/meshcore/platform.h` | Direct singleton `meshcore_platform_*` hook declarations, telemetry/request structs, host policy/data lookup, protocol callbacks, and event publication handlers. | Canonical platform hook declaration owner. |

The public contract now separates types, callable singleton runtime APIs,
and direct platform hook functions. Full-runtime hosts implement the
`meshcore_platform_*` hook symbols declared by `meshcore/platform.h`;
`meshcore_init()` initializes the single process-wide runtime against those
linked hooks. The old flat compatibility include shims have been retired.

## Direct Platform Dependencies

Current generic source files that route host-facing behavior through the
explicit platform dispatch bridge:

| Area | Files | Notes |
| --- | --- | --- |
| Clock/RNG/radio primitives | `src/core/meshcore_clock.c`, `src/core/meshcore_rng.c`, `src/core/meshcore_radio.c` | Primitive wrappers call `meshcore_platform_bridge_*`, which forwards to linked `meshcore_platform_*` hooks. |
| Crypto helpers | `src/core/meshcore_utils.c`, `src/support/crypto/*` | Crypto helpers route through the platform bridge crypto functions; bundled crypto code is support, not platform policy. |
| Protocol type headers | `src/core/meshcore_packet.h`, `src/core/meshcore_identity.h`, `src/core/meshcore_group_channel.h` | Shared sizes/types now come from `meshcore/types.h`. |
| Mesh callbacks/policy | `src/core/meshcore_mesh.c`, `src/core/meshcore_dispatcher.c`, `src/runtime/meshcore.c`, `src/runtime/meshcore_runtime_request.c`, `src/runtime/meshcore_runtime_pending.c` | Host policy/data/event calls route through `meshcore_platform_bridge_*`; no generic runtime fallback calls to legacy PAL symbols remain. |

No generic C source currently includes Zephyr, Meshbus, Arduino, board,
storage, Bluetooth, shell, or UI headers.

## Source Manifest

`cmake/meshcore_sources.cmake` is now the canonical source
manifest. Full-runtime consumers should use `MESHCORE_RUNTIME_LIBRARY_SOURCES`;
focused tests should use smaller manifest groups instead of spelling source
paths directly.

The manifest is checked by `tools/check_source_manifest.py`, which scans
`src`. Boundary tests that only compile public headers are not full library
consumers.

Manual complete source lists should not return. A direct source path in a
focused test should be treated as drift unless the file is test-local or
outside the generic library manifest roots.

## Runtime State And Memory Ownership

Runtime state now lives behind a private current-context helper. The public ABI
continues to expose a singleton runtime facade because Meshbus and existing
applications still depend on the process-wide `meshcore_*` entry points. The
internal shape is ready for a future accepted context API without making a
breaking ABI decision in this phase.

Packet queue managers use embedded fixed arenas instead of generic-library
`calloc/free`. Oversized pool requests fail deterministically by leaving the
manager uninitialized; runtime request tests cover both normal pool exhaustion
and oversized arena requests.

## Test-Only Runtime Symbols

The production runtime no longer compiles these test-facing symbol groups
unless `MESHCORE_ENABLE_TEST_HOOKS` is defined by a test target:

- `meshcore_test_runtime_is_initialized`
- `meshcore_test_runtime_context_is_default`
- `meshcore_test_runtime_dispatcher_*`
- `meshcore_test_runtime_last_now_ms_get`
- `meshcore_test_runtime_expected_ack_*`
- `meshcore_test_runtime_pending_*`
- `meshcore_test_runtime_simulate_*`

They remain useful for runtime oracle coverage, but are no longer part of
normal production library linkage.

## Duplicated Or Boundary-Sensitive Data

Known duplication or conversion pressure:

- Meshbus public constants mirror generic MeshCore limits, but
  `subsys/meshbus/services/meshcore/meshcore_adapter.h` now validates the
  mirrored ABI constants with `BUILD_ASSERT`.
- Path length and unknown-path sentinel handling exists in generic runtime and
  Meshbus host-adapter projection. Meshbus service API validation now uses
  private adapter helpers for path-length decoding and unknown-path handling.
- `MESHCORE_NODE_KEY_PREFIX_BYTES` is guarded in the canonical public
  `meshcore/types.h` header.
- Runtime request validation and Meshbus request validation intentionally
  overlap while sharing limits from the generic public contract.

Phase 7 reduced host-side duplication by making Meshbus implement the explicit
platform contract and by validating mirrored public constants in a private
adapter header.

## Gap Report

| Layer | Upstream evidence | Target C surface | Expected behavior | Migration risk | Validation need | Boundary notes |
| --- | --- | --- | --- | --- | --- | --- |
| Protocol core | Top-level `.reference/meshcore/src` files | `core` responsibility area and protocol API map | Core packet, identity, dispatcher, mesh, and utility behavior remains traceable. | Moving helper-derived behavior into core hides evidence ownership. | Protocol API map plus focused protocol Twister suites. | Helpers promoted to support must stay documented separately. |
| Support helpers | Selected `src/helpers` wire/data helpers | `support` responsibility area | Packet-visible helper formats remain available without importing host/application helpers. | Over-promoting helper objects can leak application state into generic ABI. | Support/helper parity tests and sync classification. | Support modules may be Layer 1 tests but are not protocol-core sources. |
| Runtime | `BaseChatMesh` and companion example behavior | `runtime` public requests and event ingress | Host-driven runtime preserves observable upstream behavior. | Hidden host callbacks or unclassified request surfaces make runtime behavior hard to audit. | Runtime API map and oracle tests. | Runtime owns event publication; protocol core should not own host policy. |
| Platform boundary | Current HAL/PAL headers plus host integration needs | Direct `meshcore_platform_*` hook contract | Hosts can see required primitives/policy/events from include contracts alone. | Exposing unused or ambiguous host hooks can make platform implementations provide behavior the runtime does not consume. | Boundary tests, runtime platform tests, and Meshbus service Twister. | Flat wrappers are retired; callers use canonical nested headers. |
| Build/sync | Current CMake lists and upstream reference | Source manifest plus sync report tools | New files and upstream changes become visible drift. | Manual lists can silently omit files after moves. | Manifest check, lock check, API map checks. | Source manifest should feed tests and Meshbus builds. |
| Host adapter | Meshbus service and companion frames | Meshbus platform-hook implementation outside this repository | ZBus, settings, companion protocol, and board policy remain host-owned. | Duplicated constants and path sentinels can drift from generic limits. | Meshbus service Twister plus generic library Twister. | Do not move Meshbus API ownership into generic library. |
