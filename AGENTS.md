# MeshCore C Library Agent Rules

This repository is the platform-neutral C MeshCore protocol/runtime library.
Treat this file as the durable local contract for the current implementation.

The goal is a platform-neutral C MeshCore protocol library that remains
wire-compatible with upstream Arduino MeshCore while letting the host own
scheduling, storage, transport, and hardware integration.

## Architecture Source Of Truth

The source of truth for design and compatibility is:

1. upstream protocol API evidence from `.reference/meshcore/src`
2. upstream runtime behavior evidence from `.reference/meshcore/examples`
3. the current C layering described in `ARCHITECTURE.md`
4. the current `include/`, `src/core`, `src/support`, `src/runtime`,
   `src/platform`, and `tests/` implementation

Current source and tests may be used as implementation evidence, but upstream
wire/runtime evidence wins when there is a compatibility disagreement.

## Layer Model

Use the logical layers below for every review, rewrite task, and implementation
prompt. The physical implementation lives under `src/` by responsibility.

### Layer 1: Protocol Core And Support

`src/core/` is the upstream class-to-C API translation layer.

Its purpose is 1:1 traceability at the API-design level against protocol-core
classes under top-level `.reference/meshcore/src`.

The C API may differ in ownership, allocation, lifecycle, and dispatch style,
but protocol-relevant functions, constants, fields, limits, packet layouts, and
boundary conditions must remain traceable to upstream.

`src/support/` holds explicitly promoted helper behavior, such as packet
manager/table helpers, advert data helpers, telemetry compatibility helpers,
and bundled crypto support. Support files must cite helper evidence and must
not be treated as protocol-core sources.

Layer 1 must not pull behavior from examples. Examples belong to Layer 2 unless
a specific helper is explicitly promoted to support.

### Layer 2: Runtime

`src/runtime/` is the behavior layer extracted from upstream runtime helpers and
examples.

Its purpose is to make the C library behave like a MeshCore runtime without
copying Arduino's `loop()` scheduling model.

Runtime behavior is host-driven:

- typed public requests enter the runtime
- received radio frames are injected by the host
- TX completion is injected by the host
- timer expiry is injected by the host
- callbacks publish results back to the host

Layer 2 composes Layer 1 core/support modules. It must not duplicate packet,
crypto, identity, mesh, or dispatcher rules that belong in `src/core` or
`src/support`.

Peer direct/flood decisions follow upstream MeshCore contact semantics, not raw
byte length alone. In host peer records, `out_path.size == 0` is direct
zero-hop only when `is_neighbor == true`; `out_path.size == 0` with
`is_neighbor == false` means the path is unknown and peer sends must fall back
to flood. Companion contact `out_path_len == 0xff` is the unknown/flood
sentinel; `out_path_len == 0` is zero-hop direct.

### Layer 3: Platform Boundary

Layer 3 is the host and platform boundary. The explicit public contract lives
in `include/meshcore/platform.h`; the runtime dispatch bridge lives under
`src/platform`.

The host implements platform primitives and policy hooks, then calls the public
C API. Product services, board, storage, Bluetooth, UI, and transport code must
remain outside the generic library.

## Target Tree

The current architecture is organized by responsibility:

```text
meshcore/
  include/
    meshcore/types.h    shared ABI constants and data shapes
    meshcore/runtime.h  callable runtime ABI
    meshcore/platform.h     explicit host/platform hook contract
  docs/
    porting.md          host hook and runtime API integration guide
    testing.md          independent and downstream validation guide
  examples/
    minimal_host/       public-header-only standalone host example
  src/
    core/
      packet, rng, utils, identity, clock, radio, dispatcher, mesh,
      group_channel
    support/
      packet_manager, tables, advert_data, telemetry helpers, crypto
    runtime/
      meshcore.c                    lifecycle, event ingress, timer pump
      meshcore_runtime_control.c    zero-hop control request behavior
      meshcore_runtime_event_publish.c
                                    host-facing event construction/publication
      meshcore_runtime_pending.c    ACK and pending response correlation
      meshcore_runtime_policy.c     runtime forwarding and delay policy
      meshcore_runtime_receive.c    protocol receive callback behavior
      meshcore_runtime_request.c    typed public request execution
      meshcore_runtime_bridge.h     internal protocol callback bridge
      meshcore_runtime_internal.h   private runtime state and helpers
    platform/
      meshcore_platform_bridge.c    direct platform hook dispatch bridge
  tests/
    native/             CTest entry points
    support/            native fake host/platform support
```

Do not move files mechanically just to improve shape. Preserve behavior
intentionally and keep each moved surface tied to upstream evidence.

## Upstream Evidence

Layer 1 evidence comes from:

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

Layer 2 evidence comes from:

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

The exact evidence mapping is maintained in `ARCHITECTURE.md`.

## Excluded Evidence

Do not use these upstream areas to drive generic library architecture unless a
future task explicitly promotes a behavior:

- Arduino board classes
- RadioLib concrete wrappers
- display, button, vibration, and UI helpers
- `CommonCLI` text command handling
- serial, BLE, Wi-Fi, and bridge transports
- concrete file-system or preference stores
- sensor manager implementations
- example application UI flows

These files may provide host-adapter context, but they are not generic
protocol/runtime evidence.

## Rewrite Rules

When changing architecture or code:

1. Start from the relevant layer.
2. Name the upstream evidence files first.
3. Define the target C surface before moving or rewriting code.
4. Preserve working behavior, but do not preserve old file structure by
   default.
5. Keep product-service, storage, transport, and board dependencies outside
   generic code.
6. Do not add a public ABI just because old code exposed a helper.
7. Do not remove an observable behavior unless the upstream evidence says it is
   not part of the target compatibility contract.

## Validation Strategy

Validation is organized by layer:

- protocol parity validation for Layer 1 API and packet-level behavior
- runtime oracle validation for Layer 2 observable behavior
- ABI and boundary validation for Layer 3 public contracts
- host integration validation outside this repository

Existing tests can be moved, split, or deleted only after their protected
behavior has been assigned to `protocol/`, `runtime/`, `boundary/`, host
integration, or explicit deletion.

## Gap Report Rules

When producing a gap report, use this structure:

- layer
- upstream evidence
- target C surface
- expected behavior
- migration risk
- validation need
- boundary notes

Do not report current old implementation files as if they were target
architecture. If old code is relevant, list it under migration notes only.

## Completion Checklist

Before finishing a MeshCore architecture or rewrite task:

- state which layer changed
- state which upstream evidence files were used
- state whether the work changed architecture, implementation, tests, or only
  migration notes
- state whether any old implementation detail was intentionally ignored
- state validation performed or deferred
