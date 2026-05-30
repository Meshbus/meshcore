# MeshCore C Library Architecture

This document describes the current architecture of this MeshCore C library. The
implementation is organized into core, support, runtime, and platform-boundary
responsibility areas, with upstream MeshCore used as compatibility evidence.

The library is a platform-neutral C MeshCore protocol engine with four physical
areas:

1. `src/core/`: 1:1 API-design translation from upstream top-level protocol
   classes
2. `src/support/`: explicitly promoted helper/support modules
3. `src/runtime/`: upstream-compatible behavior extracted from runtime helpers
   and examples
4. `src/platform/`: runtime dispatch bridge for the explicit platform hook
   contract

The stabilization plan refines these responsibilities toward four explicit
ownership groups:

- `core`: protocol-core translations from upstream top-level
  `.reference/meshcore/src` files;
- `support`: explicitly promoted helpers with generic packet-visible or data
  structure behavior;
- `runtime`: host-driven runtime behavior extracted from upstream helpers and
  examples;
- `platform boundary`: platform primitives, host policy/data lookup, event
  publication, and runtime dispatch through the explicit platform hook
  contract.

`UPSTREAM.md` records the evidence classification, and
`MIGRATION_INVENTORY.md` records the current stabilization drift baseline.

## Design Goals

- Preserve LoRa wire compatibility with upstream Arduino MeshCore.
- Make upstream protocol surfaces traceable at the C API-design level.
- Keep Arduino scheduling and platform code out of the generic library.
- Let hosts own scheduling, persistence, transport, and hardware integration.
- Make future upstream updates reviewable by layer.
- Keep implementation and tests reviewable by layer.

## Non-Goals

- Do not port Arduino `loop()` as the C runtime model.
- Do not make Zephyr, Meshbus, Bluetooth, storage, shell, board, or sample code
  part of the generic library.
- Do not restore the old per-class porting tracker as the primary architecture
  document.

## Layering

```text
Host application or service
  owns scheduling, storage, transport, board integration

Public include boundary
  meshcore/types.h
  meshcore/runtime.h
  meshcore/platform.h

src/runtime/
  typed requests
  radio RX/TX event ingress
  timer event ingress
  pending request lifecycle
  publish callbacks
  direct platform hook dispatch
  behavior extracted from upstream runtime helpers and examples

src/core/
  packet
  identity
  utils
  rng
  clock
  radio abstraction
  dispatcher
  mesh
  group channel

src/support/
  packet manager
  tables
  advert data
  telemetry compatibility
  crypto support

src/platform/
  direct platform hook dispatch bridge
```

Layer names are implementation responsibilities. File movement should happen
only when the target surface and migration validation are clear.

## Source Manifest

`cmake/meshcore_sources.cmake` is the canonical source manifest
for library implementation files. Full-runtime consumers such as Meshbus and
runtime oracle tests must use `MESHCORE_RUNTIME_LIBRARY_SOURCES` instead of
maintaining private complete source lists.

Focused protocol tests may use smaller manifest groups, such as
`MESHCORE_PROTOCOL_PACKET_SOURCES` or `MESHCORE_SUPPORT_ADVERT_DATA_SOURCES`,
so they remain module-level tests. New `.c` implementation files under `src`
must be added to the manifest or explicitly moved outside the generic
implementation root.

## Sync Observability

`upstream.lock` records the expected `.reference/meshcore` checkout.
`tools/upstream_lock_check.py` verifies the checkout commit and dirty state.
`tools/meshcore_sync_report.py` aggregates the lock check, evidence-file
presence, source manifest coverage, public-header ownership, include-boundary
scans, stale source-root scans, and runtime test-hook boundary checks.

Run the sync report before changing upstream evidence classification or moving
architecture boundaries. Treat failures as actionable drift that must be
classified before implementation continues.

## Layer 1: Core And Support

`src/core/` is the 1:1 traceability layer against top-level
`.reference/meshcore/src` classes. `src/support/` contains helper-derived
modules that are intentionally promoted into the generic library but remain
separate from protocol core.

The term "1:1" means API-design traceability, not mechanical C++ cloning. C may
use explicit context pointers, fixed buffers, result codes, and callback-free
helpers where C++ used classes, inheritance, or Arduino types.

The following upstream surfaces are Layer 1 inputs.

| Target protocol module | Upstream API evidence | Notes |
| --- | --- | --- |
| `packet` | `.reference/meshcore/src/Packet.h`, `.reference/meshcore/src/Packet.cpp` | Packet fields, packet types, payload layout, path/transport fields, read/write helpers, validation rules. |
| `rng` | `.reference/meshcore/src/Utils.h`, `.reference/meshcore/src/Utils.cpp` | Upstream RNG API shape and deterministic helper behavior; byte source itself remains platform-provided. |
| `utils` | `.reference/meshcore/src/Utils.h`, `.reference/meshcore/src/Utils.cpp` | Crypto helper semantics, MAC layout, hex helpers, text parsing helpers, padding and boundary behavior. |
| `identity` | `.reference/meshcore/src/Identity.h`, `.reference/meshcore/src/Identity.cpp` | Identity and local identity data shape, key handling, sign/verify behavior. |
| `clock` | `.reference/meshcore/src/Dispatcher.h`, `.reference/meshcore/src/MeshCore.h` | Millisecond and RTC abstraction semantics; time source remains HAL-provided. |
| `radio` | `.reference/meshcore/src/Dispatcher.h`, `.reference/meshcore/src/Dispatcher.cpp` | Radio abstraction expected by dispatcher; hardware operations remain HAL-provided. |
| `packet_manager` | `.reference/meshcore/src/Dispatcher.h`, `.reference/meshcore/src/Dispatcher.cpp`, `.reference/meshcore/src/helpers/StaticPoolPacketManager.h`, `.reference/meshcore/src/helpers/StaticPoolPacketManager.cpp` | Support module for packet queueing, allocation, delayed routing, outbound ownership, capacity behavior. |
| `dispatcher` | `.reference/meshcore/src/Dispatcher.h`, `.reference/meshcore/src/Dispatcher.cpp` | Receive, send, delayed send, ACK, timeout, CAD, retry, duty-budget and routing behavior. |
| `mesh` | `.reference/meshcore/src/Mesh.h`, `.reference/meshcore/src/Mesh.cpp` | Mesh packet construction, flood/direct routing, decrypt/dispatch, path handling, callbacks to runtime/PAL. |
| `tables` | `.reference/meshcore/src/Mesh.h`, `.reference/meshcore/src/Mesh.cpp`, `.reference/meshcore/src/helpers/SimpleMeshTables.h` | Support module for peer/channel lookup semantics and minimal table behavior required by protocol logic. |
| `group_channel` | `.reference/meshcore/src/Mesh.h`, `.reference/meshcore/src/Mesh.cpp` | Channel secret/hash data shape and group payload matching semantics. |
| `advert_data` | `.reference/meshcore/src/helpers/AdvertDataHelpers.h`, `.reference/meshcore/src/helpers/AdvertDataHelpers.cpp` | Support module for builder/parser behavior for advert application data. |
| `text_data` | `.reference/meshcore/src/helpers/TxtDataHelpers.h`, `.reference/meshcore/src/helpers/TxtDataHelpers.cpp` | Support/runtime evidence for TXT/GRP wire text behavior used by runtime messages. |

### Layer 1 Boundary

Layer 1 must avoid host policy. It should not know where identities, peers,
channels, telemetry, settings, or contacts are stored. Those are supplied
through runtime or PAL boundaries.

Layer 1 should not read from examples. If an example reveals a missing helper
requirement, promote the exact helper behavior into `src/support` explicitly
and cite the upstream source file that defines it.

## Layer 2: Runtime

`src/runtime/` translates upstream runtime behavior into a host-driven C
runtime.

The runtime is not a thread, not a Zephyr service, and not an Arduino loop. It
is a deterministic event pump entered by the host.

Target runtime responsibilities:

- initialize and reset runtime state
- accept public typed requests
- convert requests into protocol operations
- accept raw radio RX frames from the host
- accept TX completion from the host
- accept timer expiry from the host
- maintain pending request correlation
- publish observable results through explicit platform events
- schedule next work through host callbacks or explicit deadline reporting

### Runtime State And Memory Ownership

The public runtime ABI remains a singleton facade. Internally, the runtime
state is held in a `struct meshcore_runtime` context selected through the
private `meshcore_runtime_context_current()` helper. The current implementation
selects the default process-wide context for all public `meshcore_*` entry
points; no public multi-instance lifecycle API is exposed in this phase.

Runtime packet ownership uses fixed arenas instead of heap allocation.
`meshcore_packet_queue_manager_prepare()` binds each queue manager to storage
embedded in the manager object and rejects pool sizes larger than
`MESHCORE_PACKET_QUEUE_MANAGER_MAX_POOL_SIZE`. Full runtime builds may size the
arena with `MESHCORE_RUNTIME_PACKET_POOL_SIZE`; Meshbus passes its packet-pool
Kconfig value into both macros.

White-box runtime inspection symbols are test-only. Test targets that need
`meshcore_test_runtime_*` hooks compile the runtime sources with
`MESHCORE_ENABLE_TEST_HOOKS`; production consumers of
`MESHCORE_RUNTIME_LIBRARY_SOURCES` do not link those symbols.

Layer 2 behavior evidence comes from these upstream files.

| Runtime behavior area | Upstream evidence | Notes |
| --- | --- | --- |
| Base chat request and receive behavior | `.reference/meshcore/src/helpers/BaseChatMesh.h`, `.reference/meshcore/src/helpers/BaseChatMesh.cpp` | Main behavior source for advert, peer message, group message, request, path, trace, telemetry and callback semantics. |
| Text/group payload behavior | `.reference/meshcore/src/helpers/TxtDataHelpers.h`, `.reference/meshcore/src/helpers/TxtDataHelpers.cpp` | Runtime message payload encoding and decoding behavior. |
| Advert payload behavior | `.reference/meshcore/src/helpers/AdvertDataHelpers.h`, `.reference/meshcore/src/helpers/AdvertDataHelpers.cpp` | Runtime advert app-data encoding and parsing. |
| Contact and channel protocol fields | `.reference/meshcore/src/helpers/ContactInfo.h`, `.reference/meshcore/src/helpers/ChannelDetails.h` | Evidence for the peer identity/path and channel secret/hash fields consumed by runtime behavior. The caller owns any Contact/Channel business abstraction. |
| Companion runtime flow | `.reference/meshcore/examples/companion_radio/main.cpp` | Example-level behavior flow and request usage. |
| Companion mesh subclass behavior | `.reference/meshcore/examples/companion_radio/MyMesh.h`, `.reference/meshcore/examples/companion_radio/MyMesh.cpp` | Concrete behavior hooks and role-specific runtime decisions. |
| Companion preferences and storage context | `.reference/meshcore/examples/companion_radio/NodePrefs.h`, `.reference/meshcore/examples/companion_radio/DataStore.h`, `.reference/meshcore/examples/companion_radio/DataStore.cpp` | Caller-owned context only; do not import contact/channel abstractions or persistence implementation. |

### Runtime Boundary

Runtime may depend on `src/core`, `src/support`, and the explicit platform
hook abstraction. It must not depend on a concrete host.

Runtime should expose stable public operations for:

- lifecycle
- radio RX injection
- radio TX completion
- timer expiry
- local advert request
- peer advert replay
- direct peer message
- channel message
- path discovery
- trace path
- telemetry request
- generic binary request only if the upstream behavior is intentionally kept

Public C function names live in `include/meshcore/runtime.h`. Do not add
public names solely because an upstream class or old helper exposed a similar
method.

## Layer 3: Platform Boundary

The canonical host boundary is `meshcore/platform.h`. It defines
the direct singleton `meshcore_platform_*` hook functions and shared host data
shapes required by the full runtime. A platform that links the runtime must
provide those hook symbols directly; unsupported optional features are explicit
host stubs, not weak library fallbacks or runtime-installed function tables.

The runtime routes platform primitives, host policy/data lookup, dispatcher
diagnostics, mesh policy hooks, and event publication through
`src/platform/meshcore_platform_bridge.c`. That bridge calls the canonical
platform hooks declared by `meshcore/platform.h` and must not install or dispatch
through runtime-installed function tables.

The public include directory is intentionally split by platform role:

- `meshcore/platform.h` is the host-implemented API surface.
- `meshcore/runtime.h` is the host-callable runtime API surface.
- `meshcore/types.h` owns shared ABI constants and data shapes.

The current public include policy is:

- `meshcore/runtime.h`, `meshcore/types.h`, and `meshcore/platform.h` are the
  canonical host-facing headers.
- `meshcore/platform.h` must not expose core-private structures. Runtime bridge
  code converts internal packets, identities, and group channels into borrowed
  public view types from `meshcore/types.h` before calling platform hooks.
- old global HAL/PAL function declarations are no longer public ABI.
  Reference tests may keep private shim declarations in
  downstream host-integration test support.
- flat compatibility include shims are retired; callers must include the
  canonical nested header for the role they need.

The Zephyr Meshbus service is an explicit host adapter. It implements the
`meshcore_platform_*` hooks in `subsys/meshbus/services/meshcore` and keeps
ZBus channels, settings, companion framing, Meshbus public APIs, and board
policy outside this repository. Meshbus hook implementations call
Meshbus-private `meshbus_meshcore_*` helpers declared by `meshcore_host.h`.

### `meshcore/runtime.h`

The callable runtime ABI lives in `meshcore/runtime.h`. It should contain:

- lifecycle entrypoints
- event injection entrypoints
- typed request entrypoints
- callback or deadline contract needed by host scheduling

It must not expose Zephyr, Meshbus, Arduino, storage, Bluetooth, board, or test
types.

### `meshcore/platform.h`

The canonical host implementation contract lives in `meshcore/platform.h`.
Platform hook categories include:

- monotonic time
- RTC time
- random bytes
- radio send and radio state primitives
- airtime/scoring helpers when required by dispatcher compatibility
- cryptographic primitives required by protocol helpers
- telemetry sensor fetch only when the runtime answers upstream telemetry
  requests directly

- local node identity and profile
- peer identity/path lookup and path update
- channel secret/hash lookup
- host encryption hooks if keys or storage are host-owned
- dispatcher policy knobs
- message, ACK, advert, path, trace, telemetry, and error publication

Hook functions should stay small platform primitives, policy lookups, or event
publication bridges. They may adapt Meshbus or another host internally, but
those types must not appear in the generic header. Contact and Channel business
objects remain caller-owned and must not become MeshCore library data
projections.

## Upstream Sync Workflow

The locked upstream reference state is recorded in `upstream.lock`.

When upstream changes, review by layer.

### Step 1: Protocol API Surface

Compare `.reference/meshcore/src` against the Layer 1 table.

For each changed upstream class/helper:

- identify changed functions, constants, fields, and boundary behavior
- decide whether the target C protocol API changes
- decide whether runtime behavior is affected
- decide whether the platform contract needs a new primitive or policy hook

### Step 2: Runtime Behavior Surface

Compare the Layer 2 evidence files against target runtime behavior.

Track observable behavior:

- advert publish and replay
- direct message send
- flood message send
- channel message send
- ACK correlation
- path discovery
- trace request and response
- telemetry request and response
- timeout cleanup
- malformed or unexpected inbound packets

Do not import example UI, transport, board, or persistence implementation into
the runtime.

### Step 3: Boundary Surface

If an upstream behavior needs host data or platform capability, classify it as:

- existing HAL primitive
- new HAL primitive
- existing PAL policy/data hook
- new PAL policy/data hook
- host integration outside this repository
- intentionally unsupported behavior

## P2 Runtime Sync Ledger

The P1 runtime capability gaps are the protocol-level surfaces required for
current Meshbus interop: generic binary request/response, channel/group binary
datagram, and raw/control data. Remaining P2 work is tracked by boundary, not
by copying more upstream application objects into the generic library.

| Layer | Upstream evidence | Target C surface | Expected behavior | Migration risk | Validation need | Boundary notes |
| --- | --- | --- | --- | --- | --- | --- |
| Layer 2 runtime | `.reference/meshcore/src/helpers/BaseChatMesh.cpp`, `.reference/meshcore/examples/companion_radio/MyMesh.cpp` | `meshcore_node_binary_request*`, PAL binary response event | Binary request sends upstream-compatible `REQ`; matching `RESPONSE` with the same tag publishes opaque response bytes. | Wrong tag/peer handling can break request correlation or clear pending state too early. | Runtime request tests for wrong-tag then matching-tag response; oracle send-side request parity. | Keep response payload opaque. Higher-level RPC schemas belong to the caller. |
| Layer 2 runtime | `.reference/meshcore/src/helpers/BaseChatMesh.cpp`, `.reference/meshcore/examples/companion_radio/MyMesh.cpp` | `meshcore_channel_data_send`, PAL channel data event | `GRP_DATA` encodes `data_type`, `data_len`, and payload; unknown path floods, known path sends direct. | Treating this as text message or channel-store API would mix business models into protocol runtime. | Oracle send-side parity for flood/direct; runtime receive tests for malformed and valid datagrams. | Channel secret/hash is protocol input. Channel business objects remain caller-owned. |
| Host integration | `.reference/meshcore/examples/companion_radio/MyMesh.cpp` | `include/zephyr/meshbus/meshcore.h`, Companion command/push adapters | Companion low-level command frames map to Meshbus request APIs and response push frames. | Frame-shape drift breaks phone/app compatibility even when this library's wire behavior is correct. | Meshbus companion protocol tests for commands, errors, and push frames. | This stays outside this repository; it is a Meshbus service adapter. |
| Meshbus service | Upstream application behavior only when it uses MeshCore wire features | Message or caller-owned service APIs, not generic MeshCore ABI | Message pagination, file chunks, plugin RPC, or app protocols choose binary request or channel datagram as transport. | Putting pagination in this library would make host storage and message models part of the protocol ABI. | Service-level tests once a concrete pagination contract is defined. | This library provides opaque transport; Meshbus or the app owns schemas, storage, retries, and UX. |
| Release hardening | Current C ABI and host embedding requirements | Future ABI milestone | Public multi-instance contexts and host-native packaging should be decided before a standalone library ABI release. | Early ABI publication would make global-state decisions hard to change. | Boundary tests plus host-native build/sanitizer coverage in a separate release-hardening phase. | Not a runtime sync blocker for current Meshbus integration. |

## Migration Guidance

When changing implementation files:

- preserve working behavior by comparing old behavior against upstream evidence
- move or delete old code only after assigning it to a target layer
- avoid carrying old helper names, global state, queues, or tests into the new
  design without a layer reason
- do not treat old tests as the only contract; use them to discover coverage
  that should exist in the layer validation suite

## Validation Model

Validation should be designed after the target surfaces are defined.

Expected validation categories:

- Layer 1 protocol parity tests against upstream class/helper behavior
- Layer 2 runtime oracle tests against upstream behavior evidence
- Layer 3 ABI and boundary tests for host-visible contracts
- host integration tests outside this repository

Detailed independent test instructions belong in `docs/testing.md`.

## Difference Classification

Use these labels when planning or reviewing architecture changes:

- `target`: required by the new architecture
- `migration input`: useful old behavior or code, not an architecture rule
- `upstream evidence`: source file that defines compatibility behavior
- `host boundary`: belongs in the platform contract or host integration
- `excluded`: Arduino/platform/example implementation detail
- `deferred`: intentionally not in the current pass

## Design Invariants

- The generic library remains plain C.
- Layer 1 follows upstream protocol API evidence.
- Layer 2 follows upstream runtime behavior evidence.
- Layer 3 hides platform and host implementation details.
- Scheduling remains host-owned.
- Storage remains host-owned.
- Transport remains host-owned.
- `.reference/meshcore` remains read-only.
- Current code and tests implement the architecture, but upstream compatibility
  evidence remains the arbiter for protocol/runtime behavior.
